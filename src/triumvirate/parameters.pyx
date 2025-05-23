"""
Parameter Set (:mod:`~triumvirate.parameters`)
==============================================

Configure program parameter set.

.. autosummary::
    NonParameterError
    InvalidParameterError
    ParameterSet
    fetch_paramset_template

"""
import warnings
from pathlib import Path
from pprint import pformat, pprint

import numpy as np
import yaml

from .parameters cimport CppParameterSet


# NOTE: Nested entries must not have a non-`None` default value. Instead,
# provide all the nested entries with the bottom-level entry possibly `None`.
_TMPL_PARAM_DICT = {
    'directories': {
        'catalogues': None,
        'measurements': None,
    },
    'files': {
        'data_catalogue': None,
        'rand_catalogue': None,
    },
    'catalogue_columns': [],
    'catalogue_dataset': None,
    'tags': {
        'output': None,
    },
    'boxsize': {'x': None, 'y': None, 'z': None},
    'ngrid': {'x': None, 'y': None, 'z': None},
    'expand': 1.,
    'alignment': 'centre',
    'padscale': 'box',
    'padfactor': None,
    'assignment': 'tsc',
    'interlace': False,
    'catalogue_type': None,
    'statistic_type': None,
    'degrees': {'ell1': None, 'ell2': None, 'ELL': None},
    'wa_orders': {'i': None, 'j': None},
    'form': 'diag',
    'norm_convention': 'particle',
    'binning': 'lin',
    'range': [None, None],
    'num_bins': None,
    'idx_bin': None,
    'cutoff_nyq': None,
    'fftw_scheme': 'measure',
    'use_fftw_wisdom': False,
    'save_binned_vectors': False,
    'verbose': 20,
    'progbar': False,
}


class NonParameterError(ValueError):
    """:exc:`ValueError` raised when a program parameter is non-existent.

    """
    pass


class InvalidParameterError(ValueError):
    """:exc:`ValueError` raised when a program parameter is invalid.

    """
    pass


cdef class ParameterSet:
    """Parameter set.

    This reads parameters from a file or a :class:`dict` object, stores
    and prints out the extracted parameters, and validates the parameters.

    Parameters
    ----------
    param_filepath : str or :class:`pathlib.Path`, optional
        Parameter file path.  This should point to a YAML file.
        If `None` (default), `param_dict` should be provided; otherwise
        `param_dict` should be `None`.
    param_dict : dict, optional
        Parameter dictionary (nested).  If `None` (default),
        `param_filepath` should be provided; otherwise
        `param_filepath` should be `None`.
    logger : :class:`logging.Logger`, optional
        Program logger (default is `None`).

    Raises
    ------
    ValueError
        If neither or both of `param_filepath` and `param_dict`
        is/are `None`.


    .. tip::

        Use :func:`~triumvirate.parameters.fetch_paramset_template()` to
        generate a template parameter file (with explanatory text).

    """

    def __cinit__(self, param_filepath=None, param_dict=None, logger=None):

        self.thisptr = new CppParameterSet()

        self._logger = logger

        if (param_filepath is not None and param_dict is not None) \
                or (param_filepath is None and param_dict is None):
            raise ValueError(
                "One and only one of `param_filepath` and `param_dict` "
                "should be set."
            )

        self._params = {}
        if param_filepath is not None:
            self._source = str(Path(param_filepath).absolute())
            with open(param_filepath, 'r') as param_file:
                self._params = yaml.load(param_file, Loader=yaml.SafeLoader)
        if param_dict is not None:
            self._source = 'dict'
            self._params = param_dict

        self._original = True
        self._validity = False

        self._parse_params()  # validate

    def __dealloc__(self):
        del self.thisptr

    def __str__(self):
        return "\n".join([
            f"ParameterSet(source={self._source}, original={self._original})",
            f"{pformat(self._params, sort_dicts=False)}",
        ])

    def __getitem__(self, key):
        """Return parameter value by key like :class:`dict`.

        Parameters
        ----------
        key : str
            Parameter name.

        Returns
        -------
        Parameter value.

        """
        try:
            return self._params[key]
        except KeyError:
            raise NonParameterError(f"Non-existent parameter: '{key}'.")

    def __setitem__(self, key, val):
        """Set parameter value by key like :class:`dict`.

        Parameters
        ----------
        key : str
            Parameter name.
        val :
            Parameter value.

        """
        self._params.update({key: val})
        self._parse_params()  # validate
        self._original = False

    def __getattr__(self, name):
        """Get parameter value as an attribute.

        Parameters
        ----------
        name : str
            Parameter name.

        Returns
        -------
        Parameter value.

        """
        try:
            return self.__getitem__(name)
        except (KeyError, NonParameterError) as err:
            raise AttributeError(str(err))

    def __setattr__(self, name, value):
        """Set parameter value as an attribute.

        Parameters
        ----------
        name : str
            Parameter name.
        value :
            Parameter value.

        """
        self.__setitem__(name, value)

    def names(self):
        """Return the full set of top-level parameter names like
        :meth:`dict.keys`.

        Returns
        -------
        :obj:`dict_keys`
            Top-level parameter names as dictionary keys.

        """
        return self._params.keys()

    def items(self):
        """Return the full set of parameter entries like
        :meth:`dict.items`.

        Returns
        -------
        :obj:`dict_items`
            Parameters as dictionary items.

        """
        return self._params.items()

    def get(self, key, *default_val):
        """Return a possibly non-existent top-level parameter entry like
        :meth:`dict.get`.

        Parameters
        ----------
        key : str
            Top-level parameter name, possibly non-existent.
        default_val :
            Default value if the entry does not exist.

        Returns
        -------
        object
            Top-level parameter value, `None` if non-existent.

        """
        return self._params.get(key, *default_val)

    def update(self, *args, **kwargs):
        """Update parameter set like :meth:`dict.update`.

        """
        self._params.update(*args, **kwargs)
        self._parse_params()  # validate
        self._original = False

    def print(self):
        """Print the parameters as a dictionary
        with :func:`pprint.pprint`.

        """
        pprint(self._params, sort_dicts=False)

    def save(self, filepath=None):
        """Save validated parameters to a YAML file.

        Parameters
        ----------
        filepath : str or :class:`pathlib.Path`, optional
            Saved file path.  If `None` (default), parameters are saved
            to a default file path in the output measurement directory.

        """
        if filepath is None:
            filepath = Path(
                self._params['directories']['measurements'],
                "parameters_used{}.yml".format(self._params['tags']['output'])
            )

        with open(filepath, 'w') as printout_file:
            yaml.dump(
                self._params, printout_file,
                sort_keys=False,
                default_flow_style=False  # CAVEAT: discretionary
            )

        if self._logger:
            self._logger.info("Saved parameters to %s.", filepath)

    def _parse_params(self):
        """Parse the parameter set into the corresponding
        C++ parameter class.

        """
        # ----------------------------------------------------------------
        # Parsing
        # ----------------------------------------------------------------

        # -- I/O ---------------------------------------------------------

        # Directories, files and tags default to empty if unset.
        # Full path concatenation is handled by C++.

        # Attribute string parameters.
        try:
            if self._params['directories']['catalogues'] is None:
                self._params['directories']['catalogues'] = ""
                self.thisptr.catalogue_dir = "".encode('utf-8')
            else:
                self.thisptr.catalogue_dir = \
                    self._params['directories']['catalogues'].encode('utf-8')
        except KeyError:
            self.thisptr.catalogue_dir = "".encode('utf-8')

        try:
            if self._params['directories']['measurements'] is None:
                self._params['directories']['measurements'] = ""
                self.thisptr.measurement_dir = "".encode('utf-8')
            else:
                self.thisptr.measurement_dir = \
                    self._params['directories']['measurements'].encode('utf-8')
        except KeyError:
            self.thisptr.measurement_dir = "".encode('utf-8')

        try:
            if self._params['files']['data_catalogue'] is None:
                self._params['files']['data_catalogue'] = ""
                self.thisptr.data_catalogue_file = "".encode('utf-8')
            else:
                self.thisptr.data_catalogue_file = \
                    self._params['files']['data_catalogue'].encode('utf-8')
        except KeyError:
            self.thisptr.data_catalogue_file = "".encode('utf-8')

        try:
            if self._params['files']['rand_catalogue'] is None:
                self._params['files']['rand_catalogue'] = ""
                self.thisptr.rand_catalogue_file = "".encode('utf-8')
            else:
                self.thisptr.rand_catalogue_file = \
                    self._params['files']['rand_catalogue'].encode('utf-8')
        except KeyError:
            self.thisptr.rand_catalogue_file = "".encode('utf-8')

        # try:
        #     self.thisptr.catalogue_columns = \
        #         "".join(self._params['catalogue_columns']).encode('utf-8')
        # except TypeError as err:
        #     if self._params['catalogue_columns'] is None:
        #         self.thisptr.catalogue_columns = "".encode('utf-8')
        #     else:
        #         raise err

        try:
            if self._params['tags']['output'] is None:
                self._params['tags']['output'] = ''
                self.thisptr.output_tag = ''.encode('utf-8')
            else:
                self.thisptr.output_tag = \
                    self._params['tags']['output'].encode('utf-8')
        except KeyError:
            self.thisptr.output_tag = ''.encode('utf-8')

        # -- Mesh sampling -----------------------------------------------

        # Attribute numerical parameters.
        if None in self._params['boxsize'].values():
            warnings.warn(
                "`boxsize` parameters are unset. In this case, "
                "the box size will be computed using the particle "
                "coordinate spans and the box expansion factor.",
            )
        else:
            self.thisptr.boxsize = [
                float(self._params['boxsize']['x']),
                float(self._params['boxsize']['y']),
                float(self._params['boxsize']['z']),
            ]

        if None in self._params['ngrid'].values():
            warnings.warn(
                "`ngrid` parameters are unset. In this case, "
                "the grid cell number will be computed using the box size "
                "and the Nyquist cutoff.",
            )
        else:
            self.thisptr.ngrid = [
                self._params['ngrid']['x'],
                self._params['ngrid']['y'],
                self._params['ngrid']['z'],
            ]

        if (expand_ := self._params.get('expand')) is not None:
            self.thisptr.expand = expand_

        if (padfactor_ := self._params.get('padfactor')) is not None:
            self.thisptr.padfactor = padfactor_

        # Attribute string parameters.
        if (alignment_ := self._params.get('alignment')) is not None:
            self.thisptr.alignment = alignment_.lower().encode('utf-8')
        if (padscale_ := self._params.get('padscale')) is not None:
            self.thisptr.padscale = padscale_.lower().encode('utf-8')
        if (assignment_ := self._params.get('assignment')) is not None:
            self.thisptr.assignment = assignment_.lower().encode('utf-8')
        if (
            interlace_ := self._params.get('interlace')
        ) is not None:  # possibly convert from bool
            self.thisptr.interlace = str(interlace_).lower().encode('utf-8')

        # Attribute derived parameters.
        try:
            self._params['volume'] = np.prod(list(
                self._params['boxsize'].values()
            ))
        except TypeError:
            self._params['volume'] = 0.
        self.thisptr.volume = self._params['volume']

        try:
            self._params['nmesh'] = np.prod(list(
                self._params['ngrid'].values()
            ))
        except TypeError:
            self._params['nmesh'] = 0
        self.thisptr.nmesh = self._params['nmesh']

        # -- Measurement -------------------------------------------------

        # Attribute numerical parameters.
        # Although these parameters may not have to be set, they must be
        # present in any parameter file or dictionary.
        try:
            if self._params['degrees']['ell1'] is not None:
                self.thisptr.ell1 = self._params['degrees']['ell1']
            if self._params['degrees']['ell2'] is not None:
                self.thisptr.ell2 = self._params['degrees']['ell2']
            if self._params['degrees']['ELL'] is not None:
                self.thisptr.ELL = self._params['degrees']['ELL']
        except KeyError:
            raise InvalidParameterError(
                "`degrees` parameters must be present even if left unset."
            )

        try:
            if self._params['wa_orders']['i'] is not None:
                self.thisptr.i_wa = self._params['wa_orders']['i']
            if self._params['wa_orders']['j'] is not None:
                self.thisptr.j_wa = self._params['wa_orders']['j']
        except KeyError:
            raise InvalidParameterError(
                "`wa_orders` parameters must be present even if left unset."
            )

        try:
            if self._params['range'] is None or None in self._params['range']:
                raise InvalidParameterError("`range` parameters must be set.")
        except KeyError:
            raise InvalidParameterError("`range` parameters must be set.")
        self.thisptr.bin_min = float(self._params['range'][0])
        self.thisptr.bin_max = float(self._params['range'][1])

        if (num_bins_ := self._params.get('num_bins')) is not None:
            self.thisptr.num_bins = num_bins_
        else:
            raise InvalidParameterError("`num_bins` parameter must be set.")
        if (idx_bin_ := self._params.get('idx_bin')) is not None:
            self.thisptr.idx_bin = idx_bin_

        if self._params['nmesh'] == 0:
            if (cutoff_nyq_ := self._params.get('cutoff_nyq')) is not None:
                self.thisptr.cutoff_nyq = cutoff_nyq_
                if self._params['volume'] > 0.:
                    self._params = _set_ngrid_from_cutoff(self._params)
            else:
                raise InvalidParameterError(
                    "`cutoff_nyq` parameter must be set "
                    "when any of the grid cell numbers are unset."
                )

        # Attribute string parameters.
        if (catalogue_type_ := self._params.get('catalogue_type')) is not None:
            self.thisptr.catalogue_type = \
                catalogue_type_.lower().encode('utf-8')
        if (statistic_type_ := self._params.get('statistic_type')) is not None:
            self.thisptr.statistic_type = \
                statistic_type_.lower().encode('utf-8')

        if (form_ := self._params.get('form')) is not None:
            self.thisptr.form = form_.lower().encode('utf-8')
            if form_ in {'off-diag', 'row'} \
                    and self._params.get('idx_bin') is None:
                raise InvalidParameterError(
                    "`idx_bin` parameter must be set "
                    "when `form` is 'off-diag' or 'row'."
                )

        if (
            norm_convention_ := self._params.get('norm_convention')
        ) is not None:
            self.thisptr.norm_convention = \
                norm_convention_.lower().encode('utf-8')

        if (binning_ := self._params.get('binning')) is not None:
            self.thisptr.binning = binning_.lower().encode('utf-8')

        # Attribute otherwise-derived parameters.
        assignment_order_ = self._params.get('assignment_order', 0)
        space_ = self._params.get('space', '').lower()
        npoint_ = self._params.get('npoint', '').lower()
        if assignment_order_ in {1, 2, 3, 4}:
            self.thisptr.assignment_order = assignment_order_
        if space_ in {'fourier', 'config'}:
            self.thisptr.space = space_.encode('utf-8')
        if npoint_ in {'2pt', '3pt'}:
            self.thisptr.npoint = npoint_.encode('utf-8')

        # -- Misc --------------------------------------------------------

        if (fftw_scheme_ := self._params.get('fftw_scheme')) is not None:
            self.thisptr.fftw_scheme = fftw_scheme_.encode('utf-8')

        if (
            use_fftw_wisdom_ := self._params.get('use_fftw_wisdom')
        ) is not None:
            self.thisptr.use_fftw_wisdom = \
                str(use_fftw_wisdom_).lower().encode('utf-8')

        if (verbose_ := self._params.get('verbose')) is not None:
            self.thisptr.verbose = verbose_

        if (progbar_ := self._params.get('progbar')) is not None:
            self.thisptr.progbar = str(progbar_).lower().encode('utf-8')

        # ----------------------------------------------------------------
        # Validation
        # ----------------------------------------------------------------

        self._validate(True)

    def _validate(self, init=False):
        """Validate extracted parameters.

        """
        try:
            self._logger.stat("Validating parameters...", cpp_state='start')
        except (AttributeError, TypeError):
            pass

        if self.thisptr.validate(init) != 0:
            raise InvalidParameterError("Invalid measurement parameters.")

        # Fetch validated parameters that have been derived or transmuted.
        # Transmuted I/O paths are not fetched.
        self._params['assignment_order'] = self.thisptr.assignment_order
        self._params['npoint'] = self.thisptr.npoint.decode('utf-8')
        self._params['space'] = self.thisptr.space.decode('utf-8')
        self._params['fftw_planner_flag'] = self.thisptr.fftw_planner_flag
        self._params['fftw_wisdom_file_f'] = \
            self.thisptr.fftw_wisdom_file_f.decode('utf-8')
        self._params['fftw_wisdom_file_b'] = \
            self.thisptr.fftw_wisdom_file_b.decode('utf-8')

        _use_fftw_wisdom = self.thisptr.use_fftw_wisdom.decode('utf-8')
        if _use_fftw_wisdom.lower() in ['false', '']:
            self._params['use_fftw_wisdom'] = False
        else:
            self._params['use_fftw_wisdom'] = _use_fftw_wisdom

        _interlace = self.thisptr.interlace.decode('utf-8')
        if _interlace.lower() == 'true':
            self._params['interlace'] = True
        elif _interlace.lower() == 'false':
            self._params['interlace'] = False
        else:
            raise RuntimeError(
                "C++ instance of trv::ParameterSet did not return "
                "a recognised 'interlace' string in {'true', 'false'}."
            )

        _progbar = self.thisptr.progbar.decode('utf-8')
        try:
            self._params['progbar'] = int(_progbar)
        except ValueError:
            if _progbar == 'true':
                self._params['progbar'] = True
            elif _progbar == 'false':
                self._params['progbar'] = False
            else:
                raise RuntimeError(
                    "C++ instance of trv::ParameterSet did not return "
                    "a recognised 'verbose' value "
                    "'true', 'false' or an integer."
                )

        self._validity = True

        try:
            self._logger.stat("... validated parameters.", cpp_state='end')
        except (AttributeError, TypeError):
            pass


def fetch_paramset_template(format, ret_defaults=False):
    """Fetch a parameter set template, either as a YAML file
    (with explanatory text) or as a dictionary.

    The returned template parameters are not validated.

    Parameters
    ----------
    format : {'text', 'dict'}
        Template format, either as file content text ('text')
        or a dictionary ('dict').
    ret_defaults : bool, optional
        If `True` (default is `False`), return a list of non-`None`
        default parameters in the template.  Only used when `format`
        is 'dict'.

    Returns
    -------
    template : str or dict
        Parameter set template as text or a dictionary.
    defaults : dict
        Parameter set default entries.  Only returned when `format` is
        'dict' and `ret_defaults` is `True`.

    Raises
    ------
    ValueError
        If `format` is neither 'text' nor 'dict'.

    """
    pkg_root_dir = Path(__file__).parent
    tml_filepath = pkg_root_dir/"resources"/"params_template.yml"

    text_template = tml_filepath.read_text()
    dict_template = yaml.load(text_template, Loader=yaml.SafeLoader)

    if not ret_defaults:
        if format.lower() == 'text':
            return text_template
        if format.lower() == 'dict':
            return dict_template

    defaults = {}
    for key, val in dict_template.items():
        if isinstance(val, (tuple, list, set, dict)):
            continue
        if val is not None:
            defaults.update({key: val})

    if format.lower() == 'text':
        return text_template, defaults
    if format.lower() == 'dict':
        return dict_template, defaults

    raise ValueError("`format` must be either 'text' or 'dict'.")


def _modify_sampling_parameters(paramset, params_sampling=None,
                                params_default=None, ret_defaults=False):
    """Modify sampling parameters in a parameter set.

    Parameters
    ----------
    paramset : dict-like
        Parameter set to be updated.
    params_sampling : dict, optional
        Dictionary containing any number of the following
        sampling parameters---

        - 'expand': float;
        - 'boxsize': [float, float, float];
        - 'ngrid': [int, int, int];
        - 'alignment': {'centre', 'pad'}
        - 'assignment': {'ngp', 'cic', 'tsc', 'pcs'};
        - 'interlace': bool;
        - 'cutoff_nyq': float;

        and exactly one of the following parameters only when 'alignment'
        is 'pad'---

        - 'boxpad': float;
        - 'gridpad': float.

        This will override the corresponding parameters in the input
        parameter set.  If `None` (default), no modification is made and
        the input `paramset` is returned.
    params_default : dict, optional
        If not `None` (default), this is a sub-dictionary of `paramset`
        and any of its entries unmodified by `params_sampling` will be
        registered as default values.
    ret_defaults : bool, optional
        If `True` (default is `False`), return a list of non-`None`
        default parameters which have not been modified.  Only used if
        `params_default` is not `None`.

    Returns
    -------
    paramset : dict-like
        Modified parameter set.
    params_default : dict, optional
        Only returned when `ret_defaults` is `True` and there are
        unmodified default parameters from  `params_default`.

    Raises
    ------
    ValueError
        When `params_default` is not a sub-dictionary of `paramset`.

    """
    if params_sampling is None:
        return paramset

    if params_default is not None:
        if not (params_default.items() <= paramset.items()):
            raise ValueError(
                "`params_default` should be a subdictionary of `paramset`."
            )

    if 'expand' in params_sampling.keys():
        paramset['expand'] = params_sampling['expand']
        if params_default: params_default.pop('expand', None)
    if 'alignment' in params_sampling.keys():
        paramset['alignment'] = params_sampling['alignment']
        if params_default: params_default.pop('alignment', None)

    if paramset.get('alignment') == 'pad':
        if 'boxpad' in params_sampling.keys() \
                and 'gridpad' in params_sampling.keys():
            raise ValueError(
                "Cannot set both 'boxpad` and 'gridpad' in `params_sampling`."
            )
    if 'boxpad' in params_sampling.keys():
        paramset['padscale'] = 'box'
        paramset['padfactor'] = params_sampling['boxpad']
        if params_default:
            params_default.pop('padscale', None)
            params_default.pop('padfactor', None)
    if 'gridpad' in params_sampling.keys():
        paramset['padscale'] = 'grid'
        paramset['padfactor'] = params_sampling['gridpad']
        if params_default:
            params_default.pop('padscale', None)
            params_default.pop('padfactor', None)

    if 'boxsize' in params_sampling.keys():
        paramset['boxsize'] = dict(zip(
            ['x', 'y', 'z'], params_sampling['boxsize']
        ))
        if params_default: params_default.pop('boxsize', None)
    if 'ngrid' in params_sampling.keys():
        paramset['ngrid'] = dict(zip(
            ['x', 'y', 'z'], params_sampling['ngrid']
        ))
        if params_default: params_default.pop('ngrid', None)
    if 'assignment' in params_sampling.keys():
        paramset['assignment'] = params_sampling['assignment']
        if params_default: params_default.pop('assignment', None)
    if 'interlace' in params_sampling.keys():
        paramset['interlace'] = params_sampling['interlace']
        if params_default: params_default.pop('interlace', None)

    if 'cutoff_nyq' in params_sampling.keys():
        paramset['cutoff_nyq'] = params_sampling['cutoff_nyq']
        if params_default: params_default.pop('cutoff_nyq', None)

    if ret_defaults:
        return paramset, params_default
    return paramset


def _modify_measurement_parameters(paramset, params_measure=None,
                                   params_default=None, ret_defaults=False):
    """Modify measurement parameters in a parameter set.

    Parameters
    ----------
    paramset : dict-like
        Parameter dictionary to be updated.
    params_measure : dict, optional
        Dictionary containing a subset of the following entries
        for measurement parameters:

        - 'ell1', 'ell2', 'ELL': int;
        - 'i_wa', 'j_wa': int;
        - 'form': {'full', 'diag', 'off-diag', 'row'};
        - 'bin_min', 'bin_max': float;
        - 'num_bins': int;
        - 'idx_bin': int.

        This will override the corresponding parameters in the input
        parameter set.  If `None` (default), no modification is made and
        the input `paramset` is returned.
    params_default : dict, optional
        If not `None` (default), this is a sub-dictionary of `paramset`
        and any of its entries unmodified by `params_measure` will be
        registered as default values.
    ret_defaults : bool, optional
        If `True` (default is `False`), return a list of non-`None`
        default parameters which have not been modified.  Only used if
        `params_default` is not `None`.

    Returns
    -------
    paramset : dict-like
        Modified parameter dictionary.
    params_default : dict, optional
        Only returned when `ret_defaults` is `True` and there are
        unmodified default parameters from `params_default`.

    Raises
    ------
    ValueError
        When `params_default` is not a subdictionary of `paramset`.

    """
    if params_measure is None:
        return paramset

    if params_default is not None:
        if not (params_default.items() <= paramset.items()):
            raise ValueError(
                "`params_default` should be a subdictionary of `paramset`."
            )

    for deg_label in ['ell1', 'ell2', 'ELL']:
        if deg_label in params_measure.keys():
            paramset['degrees'][deg_label] = params_measure[deg_label]

    for waorder_label in ['i_wa', 'j_wa']:
        if waorder_label in params_measure.keys():
            paramset['wa_orders'][waorder_label.rstrip('_wa')] = \
                params_measure[waorder_label]

    # Update 'idx_bin' before 'form' owing to dependency.
    if 'idx_bin' in params_measure.keys():
        paramset['idx_bin'] = params_measure['idx_bin']
        if params_default: params_default.pop('idx_bin', None)
    if 'form' in params_measure.keys():
        paramset['form'] = params_measure['form']
        if params_default: params_default.pop('form', None)

    if 'bin_min' in params_measure.keys():
        paramset['range'][0] = params_measure['bin_min']
    if 'bin_max' in params_measure.keys():
        paramset['range'][1] = params_measure['bin_max']
    if 'num_bins' in params_measure.keys():
        paramset['num_bins'] = params_measure['num_bins']
        if params_default: params_default.pop('num_bins', None)

    if ret_defaults:
        return paramset, params_default
    return paramset


def _set_ngrid_from_cutoff(paramset):
    """Set the mesh grid size from the Nyquist cutoff.

    Parameters
    ----------
    paramset : dict-like
        Parameter set to be updated.

    Returns
    -------
    paramset : dict-like
        Modified parameter set.

    """
    for ax in ['x', 'y', 'z']:
        if paramset['space'] == 'fourier':
            ngrid_ = int(np.ceil(
                paramset['boxsize'][ax] * paramset['cutoff_nyq'] / np.pi
            ))
        elif paramset['space'] == 'config':
            ngrid_ = int(
                2 * np.ceil(paramset['boxsize'][ax] / paramset['cutoff_nyq'])
            )
        else:
            raise ValueError(
                "Space must be either 'fourier' or 'config': "
                f"`space` = '{paramset['space']}'."
            )
        paramset['ngrid'][ax] = ngrid_ + ngrid_ % 2

    return paramset


# NOTE: Remove docstrings from Cython pseudo special functions.
vars()['__reduce_cython__'].__doc__ = None
vars()['__setstate_cython__'].__doc__ = None
