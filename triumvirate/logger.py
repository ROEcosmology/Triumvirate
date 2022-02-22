"""
Program Logger (:mod:`~triumvirate.logger`)
===========================================================================

Provide the program logger.

"""
import logging
import sys
import time

# TODO: Add C++ context to logger.


class _ElapsedLogFormatter(logging.Formatter):
    """Elapsed-time logging formatter.

    """

    _start_time = time.time()

    def format(self, record):
        """Add elapsed time in hours, minutes and seconds to the log
        record.

        Parameters
        ----------
        record : :class:`Logging.LogRecord`
            See :class:`logging.LogRecord`.

        Returns
        -------
        str
            Modified log record with elapsed time.

        """
        elapsed_time = record.created - self._start_time
        h, remainder_time = divmod(elapsed_time, 3600)
        m, s = divmod(remainder_time, 60)

        record.elapsed = "(+{}:{:02d}:{:02d})".format(int(h), int(m), int(s))

        return logging.Formatter.format(self, record)


class _CppLogAdapter(logging.LoggerAdapter):
    """C++ logging adapter.

    """

    def process(self, msg, kwargs):
        """Adapt logger message.

        Parameters
        ----------
        msg : str
            See :class:`logging.LoggerAdapter`.
        kwargs : dict
            See :class:`logging.LoggerAdapter`.

        Returns
        -------
        str
            Adapted log message with C++ state indication.

        """
        # Extract passed state variable or resort to default from `extra`.
        cpp_state = kwargs.pop('cpp_state', self.extra['cpp_state'])

        if cpp_state:
            return "(C++) %s" % msg, kwargs
        else:
            return "%s" % msg, kwargs


def setup_logger():
    """Set up and return a customised logger with elapsed time and
    C++ state indication.

    Returns
    -------
    customised_logger : :class:`logging.LoggerAdapter`
        Customised logger.

    """
    # Clear logger handlers.
    logger = logging.getLogger()
    if logger.hasHandlers():
        logger.handlers.clear()

    # Set logger handler with the customised formatter.
    logging_formatter = _ElapsedLogFormatter(
        fmt='[%(asctime)s %(elapsed)s %(levelname)s] %(message)s',
        datefmt='%Y-%m-%d %H:%M:%S'
    )

    logging_handler = logging.StreamHandler(sys.stdout)
    logging_handler.setFormatter(logging_formatter)

    logger.addHandler(logging_handler)

    # Adapt logger for C++ code indication.
    customised_logger = _CppLogAdapter(logger, {'cpp_state': False})

    return customised_logger
