/**
 * @file twopt.hpp
 * @brief Two-point correlator computations.
 *
 */

#ifndef TRIUMVIRATE_INCLUDE_TWOPT_HPP_INCLUDED_
#define TRIUMVIRATE_INCLUDE_TWOPT_HPP_INCLUDED_

#include "common.hpp"
#include "tools.hpp"
#include "particles.hpp"
#include "field.hpp"

/**
 * Power spectrum measurements.
 *
 */
struct PowspecMeasurements {
  double* kbin;  ///< central wavenumber in bins
  double* keff;  ///< effective wavenumber in bins
  std::complex<double>* pk_raw;  ///< power spectrum raw measurements
  std::complex<double>* pk_shot;  ///< power spectrum shot noise
};

/**
 * Two-point correlation function measurements.
 *
 */
struct CorrfuncMeasurements {
  double* rbin;  ///< central separation in bins
  double* reff;  ///< effective separation in bins
  std::complex<double>* xi;  /**< two-point correlation function
                                  measurements */
};

/**
 * Power spectrum window measurements.
 *
 */
struct PowspecWindowMeasurements {
  double* kbin;  ///< central wavenumber in bins
  double* keff;  ///< effective wavenumber in bins
  std::complex<double>* pk;  ///< power spectrum raw measurements
};

/**
 * Two-point correlation function window measurements.
 *
 */
struct CorrfuncWindowMeasurements {
  double* rbin;  ///< central separation in bins
  double* reff;  ///< effective separation in bins
  std::complex<double>* xi;  /**< two-point correlation function
                                  window measurements */
};

/**
 * Compute power spectrum from paired catalogues and
 * optionally save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param particles_rand (Random-source) particle container.
 * @param los_data (Data-source) particle lines of sight.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @param alpha Alpha ratio.
 * @param norm Inverse-effective-volume normalisation factor.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns powspec_out Output power spectrum measurements.
 */
PowspecMeasurements compute_powspec(
  ParticleCatalogue& particles_data, ParticleCatalogue& particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double* kbin,
  double alpha,
  double norm,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring power spectrum from "
      "data and random catalogues.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Set-up ************************************************************** */

  /// Set up input.
  int ell1 = params.ELL;

  /// Set up output.
  double* k_save = new double[params.num_kbin];
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  std::complex<double>* sn_save = new std::complex<double>[params.num_kbin];
  for (int ibin = 0; ibin < params.num_kbin; ibin++) {
    k_save[ibin] = 0.;
    pk_save[ibin] = 0.;
    sn_save[ibin] = 0.;
  }

  PowspecMeasurements powspec_out;

  /* * Measurement ********************************************************* */

  /// Compute power spectrum.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_fluctuation(
    particles_data, particles_rand, los_data, los_rand, alpha, 0, 0
  );
  dn_00.fourier_transform();

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
    dn_LM.compute_ylm_wgtd_fluctuation(
      particles_data, particles_rand, los_data, los_rand, alpha, params.ELL, M_
    );
    dn_LM.fourier_transform();

    Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
    std::complex<double> sn_amp = stats2pt.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_data, particles_rand, los_data, los_rand, alpha, params.ELL, M_
    );

    /// Compute quantity equivalent to (-1)^m_1 δ_{m_1, -M} which, after
    /// being summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1 = - ell1; m1 <= ell1; m1++) {
      double coupling = (2*params.ELL + 1) * (2*ell1 + 1)
        * wigner_3j(ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(ell1, 0, params.ELL, m1, 0, M_);

      if (fabs(coupling) < 1.e-10) {continue;}

      stats2pt.compute_ylm_wgtd_2pt_stats_in_fourier(
        dn_LM, dn_00, sn_amp, kbin, ell1, m1
      );

      for (int ibin = 0; ibin < params.num_kbin; ibin++) {
        pk_save[ibin] += coupling * stats2pt.pk[ibin];
        sn_save[ibin] += coupling * stats2pt.sn[ibin];
      }

      if (M_ == 0 && m1 == 0) {
        for (int ibin = 0; ibin < params.num_kbin; ibin++) {
          k_save[ibin] = stats2pt.k[ibin];
        }
      }
    }

    if (currTask == 0) {
      clockElapsed = double(clock() - clockStart);
      printf(
        "[STAT] (+%s) Computed power spectrum term of order `M = %d`.\n",
        calc_elapsed_time_in_hhmmss(clockElapsed).c_str(),
        M_
      );
    }
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  powspec_out.kbin = kbin;
  powspec_out.keff = k_save;
  powspec_out.pk_raw = pk_save;
  powspec_out.pk_shot = sn_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char save_filepath[1024];
    sprintf(
      save_filepath, "%s/pk%d%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(save_filepath, "w");
    for (int ibin = 0; ibin < params.num_kbin; ibin++) {
      fprintf(
        save_fileptr,
        "%.6f \t %.6f \t %.7e \t %.7e \t %.7e \t %.7e\n",
        kbin[ibin], k_save[ibin],
        norm * pk_save[ibin].real(), norm * pk_save[ibin].imag(),
        norm * sn_save[ibin].real(), norm * sn_save[ibin].imag()
      );
    }
    fclose(save_fileptr);
  }

  delete[] k_save; delete[] pk_save; delete[] sn_save;

  return powspec_out;
}

/**
 * Compute two-point correlation function from paired catalogues and
 * optionally save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param particles_rand (Random-source) particle container.
 * @param los_data (Data-source) particle lines of sight.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param rbin Separation bins.
 * @param alpha Alpha ratio.
 * @param norm Inverse-effective-volume normalisation factor.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns corrfunc_out Output two-point correlation function
 *                       measurements.
 */
CorrfuncMeasurements compute_corrfunc(
  ParticleCatalogue& particles_data, ParticleCatalogue& particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double* rbin,
  double alpha,
  double norm,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring two-point correlation function "
      "from data and random catalogues.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Set-up ************************************************************** */

  /// Set up input.
  int ell1 = params.ELL;

  /// Set up output.
  double* r_save = new double[params.num_rbin];
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int ibin = 0; ibin < params.num_rbin; ibin++) {
    r_save[ibin] = 0.;
    xi_save[ibin] = 0.;
  }

  CorrfuncMeasurements corrfunc_out;

  /* * Measurement ********************************************************* */

  /// Compute 2PCF.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_fluctuation(
    particles_data, particles_rand, los_data, los_rand, alpha, 0, 0
  );
  dn_00.fourier_transform();

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
    dn_LM.compute_ylm_wgtd_fluctuation(
      particles_data, particles_rand, los_data, los_rand, alpha, params.ELL, M_
    );
    dn_LM.fourier_transform();

    Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
    std::complex<double> sn_amp = stats2pt.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_data, particles_rand, los_data, los_rand, alpha, params.ELL, M_
    );

    /// Compute quantity equivalent to (-1)^m_1 δ_{m_1, -M} which, after
    /// being summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1 = - ell1; m1 <= ell1; m1++) {
      double coupling = (2*params.ELL + 1) * (2*ell1 + 1)
        * wigner_3j(ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(ell1, 0, params.ELL, m1, 0, M_);

      if (fabs(coupling) < 1.e-10) {continue;}

      stats2pt.compute_ylm_wgtd_2pt_stats_in_config(
        dn_LM, dn_00, sn_amp, rbin, ell1, m1
      );

      for (int ibin = 0; ibin < params.num_rbin; ibin++) {
        xi_save[ibin] += coupling * stats2pt.xi[ibin];
      }

      if (M_ == 0 && m1 == 0) {
        for (int ibin = 0; ibin < params.num_kbin; ibin++) {
          r_save[ibin] = stats2pt.r[ibin];
        }
      }
    }

    if (currTask == 0) {
      clockElapsed = double(clock() - clockStart);
      printf(
        "[STAT] (+%s) Computed two-point correlation function term of "
        "order `M = %d`.\n",
        calc_elapsed_time_in_hhmmss(clockElapsed).c_str(),
        M_
      );
    }
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  corrfunc_out.rbin = rbin;
  corrfunc_out.reff = r_save;
  corrfunc_out.xi = xi_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char save_filepath[1024];
    sprintf(
      save_filepath, "%s/xi%d%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(save_filepath, "w");
    for (int ibin = 0; ibin < params.num_rbin; ibin++) {
      fprintf(
        save_fileptr, "%.2f \t %.2f \t %.7e\n",
        rbin[ibin], r_save[ibin], norm * xi_save[ibin].real()
      );
    }
    fclose(save_fileptr);
  }

  delete[] r_save; delete[] xi_save;

  return corrfunc_out;
}

/**
 * Compute power spectrum window from a random catalogue and
 * optionally save the results.
 *
 * @param particles_rand (Random-source) particle container.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @param alpha Alpha ratio.
 * @param norm Inverse-effective-volume normalisation factor.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns powwin_out Output power spectrum window measurements.
 */
PowspecWindowMeasurements compute_powspec_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double* kbin,
  double alpha,
  double norm,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring power spectrum window from random catalogue.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Set-up ************************************************************** */

  /// Set up output.
  double* k_save = new double[params.num_kbin];
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int ibin = 0; ibin < params.num_kbin; ibin++) {
    k_save[ibin] = 0.;
    pk_save[ibin] = 0.;
  }

  PowspecWindowMeasurements powwin_out;

  /* * Measurement ********************************************************* */

  /// Normalise the normalisation.
  norm /= alpha * alpha;
  norm /= params.volume;  // QUEST: volume normalisation essential?

  /// Compute power spectrum window.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.fourier_transform();

  Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
  std::complex<double> sn_amp = stats2pt.calc_ylm_wgtd_shotnoise_for_powspec(
    particles_rand, los_rand, alpha, params.ELL, 0
  );

  stats2pt.compute_ylm_wgtd_2pt_stats_in_fourier(
    dn_00, dn_00, sn_amp, kbin, params.ELL, 0
  );

  for (int ibin = 0; ibin < params.num_kbin; ibin++) {
    k_save[ibin] = stats2pt.k[ibin];
    pk_save[ibin] += stats2pt.pk[ibin];
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  powwin_out.kbin = kbin;
  powwin_out.keff = k_save;
  powwin_out.pk = pk_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char buf[1024];
    sprintf(
      buf, "%s/pk%d_window%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(buf, "w");
    for (int ibin = 0; ibin < params.num_kbin; ibin++) {
      fprintf(
        save_fileptr, "%.6f \t %.6f \t %.7e \t %.7e\n",
        kbin[ibin], k_save[ibin],
        norm * pk_save[ibin].real(), norm * pk_save[ibin].imag()
      );
    }
    fclose(save_fileptr);
  }

  delete[] k_save; delete[] pk_save;

  return powwin_out;
}

/**
 * Compute two-point correlation function window from a random catalogue
 * and optionally save the results.
 *
 * @param particles_rand (Random-source) particle container.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @param alpha Alpha ratio.
 * @param norm Inverse-effective-volume normalisation factor.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns corrfwin_out Output two-point correlation function
 *                       window measurements.
 */
CorrfuncWindowMeasurements compute_corrfunc_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double* rbin,
  double alpha,
  double norm,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring two-point correlation function window "
			"from random catalogue.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
		);
  }

  /* * Set-up ************************************************************** */

  /// Set up input.
  int ell1 = params.ELL;

  /// Set up output.
  double* r_save = new double[params.num_rbin];
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int ibin = 0; ibin < params.num_rbin; ibin++) {
    r_save[ibin] = 0.;
    xi_save[ibin] = 0.;
  }

  CorrfuncWindowMeasurements corrfwin_out;

  /* * Measurement ********************************************************* */

  /// Normalise and then save the output.
  norm /= particles_rand.wtotal / particles_rand.wtotal;  // QUEST
  norm /= alpha * alpha;

  /// Compute 2PCF window.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.fourier_transform();

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
    dn_LM.compute_ylm_wgtd_density(
      particles_rand, los_rand, alpha, params.ELL, M_
    );
    dn_LM.fourier_transform();

    Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
    std::complex<double> sn_amp = stats2pt.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_rand, los_rand, alpha, params.ELL, M_
    );

    /// Calculate equivalent to (-1)^m_1 δ_{m_1, -M} which, after being
    /// summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1 = - ell1; m1 <= ell1; m1++) {
      double coupling = (2*params.ELL + 1) * (2*ell1 + 1)
          * wigner_3j(ell1, 0, params.ELL, 0, 0, 0)
          * wigner_3j(ell1, 0, params.ELL, m1, 0, M_);

      if (fabs(coupling) < 1.e-10) {continue;}

      stats2pt.compute_ylm_wgtd_2pt_stats_in_config(
        dn_LM, dn_00, sn_amp, rbin, ell1, m1
      );

      for (int ibin = 0; ibin < params.num_rbin; ibin++) {
        xi_save[ibin] += coupling * stats2pt.xi[ibin];
      }

      if (M_ == 0 && m1 == 0) {
        for (int ibin = 0; ibin < params.num_kbin; ibin++) {
          r_save[ibin] = stats2pt.r[ibin];
        }
      }
    }

    if (currTask == 0) {
      clockElapsed = double(clock() - clockStart);
      printf(
        "[STAT] (+%s) Computed two-point correlation function window term "
        "with order `M = %d`.\n",
        calc_elapsed_time_in_hhmmss(clockElapsed).c_str(),
        M_
      );
    }
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  corrfwin_out.rbin = rbin;
  corrfwin_out.reff = r_save;
  corrfwin_out.xi = xi_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char buf[1024];
    sprintf(
      buf, "%s/xi%d_window%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(buf, "w");
    for (int ibin = 0; ibin < params.num_rbin; ibin++) {
      fprintf(
        save_fileptr, "%.2f \t %.2f \t %.7e\n",
        rbin[ibin], r_save[ibin], norm * xi_save[ibin].real()
      );
    }
    fclose(save_fileptr);
  }

  delete[] r_save; delete[] xi_save;

  return corrfwin_out;
}

/**
 * Compute power spectrum in a periodic box and optionally
 * save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns powspec_out Output power spectrum measurements.
 */
PowspecMeasurements compute_powspec_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* kbin,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring power spectrum in a periodic box.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Set-up ************************************************************** */

  double* k_save = new double[params.num_kbin];
  std::complex<double>* sn_save = new std::complex<double>[params.num_kbin];
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int ibin = 0; ibin < params.num_kbin; ibin++) {
    k_save[ibin] = 0.;
    sn_save[ibin] = 0.;
    pk_save[ibin] = 0.;
  }

  PowspecMeasurements powspec_out;

  /* * Measurement ********************************************************* */

  /// Calculate normalisation.
  /// QUEST: Invert?
  double norm = params.volume
    / double(particles_data.ntotal) / double(particles_data.ntotal);

  /// Compute power spectrum.
  PseudoDensityField<ParticleCatalogue> dn(params);
  dn.compute_unweighted_fluctuation_insitu(particles_data, params.volume);
  dn.fourier_transform();

  std::complex<double> sn_amp = double(particles_data.ntotal);

  Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
  stats2pt.compute_ylm_wgtd_2pt_stats_in_fourier(
    dn, dn, sn_amp, kbin, params.ELL, 0
  );

  for (int ibin = 0; ibin < params.num_kbin; ibin++) {
    k_save[ibin] = stats2pt.k[ibin];
    sn_save[ibin] += double(2*params.ELL + 1) * stats2pt.sn[ibin];
    pk_save[ibin] += double(2*params.ELL + 1) * stats2pt.pk[ibin];
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  powspec_out.kbin = kbin;
  powspec_out.keff = k_save;
  powspec_out.pk_raw = pk_save;
  powspec_out.pk_shot = sn_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char buf[1024];
    sprintf(
      buf, "%s/pk%d%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(buf, "w");
    for (int ibin = 0; ibin < params.num_kbin; ibin++) {
      fprintf(
        save_fileptr,
        "%.6f \t %.6f \t %.7e \t %.7e \t %.7e \t %.7e\n",
        kbin[ibin], k_save[ibin],
        norm * pk_save[ibin].real(), norm * pk_save[ibin].imag(),
        norm * sn_save[ibin].real(), norm * sn_save[ibin].imag()
      );
    }
    fclose(save_fileptr);
  }

  delete[] k_save; delete[] pk_save; delete[] sn_save;

  return powspec_out;
}

/**
 * Compute two-point correlation function in a periodic box
 * and optionally save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param params Parameter set.
 * @param rbin Separation bins.
 * @param save If `true` (default is `false`), write computed results
 *             to the measurement output file set by `params`.
 * @returns corrfunc_out Output two-point correlation function
 *                       measurements.
 */
CorrfuncMeasurements compute_corrfunc_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* rbin,
  bool save=false
) {
  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Measuring two-point correlation function "
      "in a periodic box.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Set-up ************************************************************** */

  double* r_save = new double[params.num_kbin];
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int ibin = 0; ibin < params.num_rbin; ibin++) {
    r_save[ibin] = 0.;
    xi_save[ibin] = 0.;
  }

  CorrfuncMeasurements corrfunc_out;

  /* * Measurement ********************************************************* */

  /// Calculate normalisation.
  /// QUEST: Invert?
  double norm = params.volume
    / double(particles_data.ntotal) / double(particles_data.ntotal);

  /// Compute 2PCF.
  PseudoDensityField<ParticleCatalogue> dn(params);
  dn.compute_unweighted_fluctuation_insitu(particles_data, params.volume);
  dn.fourier_transform();

  std::complex<double> sn_amp = double(particles_data.ntotal);

  Pseudo2ptStats<ParticleCatalogue> stats2pt(params);
  stats2pt.compute_ylm_wgtd_2pt_stats_in_config(
    dn, dn, sn_amp, rbin, params.ELL, 0
  );

  for (int ibin = 0; ibin < params.num_rbin; ibin++) {
    r_save[ibin] = stats2pt.r[ibin];
    xi_save[ibin] += double(2*params.ELL + 1) * stats2pt.xi[ibin];
  }

  if (currTask == 0) {
    clockElapsed = double(clock() - clockStart);
    printf(
      "[STAT] (+%s) Computed two-point correlation function terms.\n",
      calc_elapsed_time_in_hhmmss(clockElapsed).c_str()
    );
  }

  /* * Output ************************************************************** */

  /// Fill in output struct.
  corrfunc_out.rbin = rbin;
  corrfunc_out.reff = r_save;
  corrfunc_out.xi = xi_save;

  /// Save (optionally) to output file.
  if (save) {
    /// Set output path.
    char buf[1024];
    sprintf(
      buf, "%s/xi%d%s",
      params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
    );

    /// Write output.
    FILE* save_fileptr = fopen(buf, "w");
    for (int ibin = 0; ibin < params.num_rbin; ibin++) {
      fprintf(
        save_fileptr, "%.2f \t %.2f \t %.7e\n",
        rbin[ibin], r_save[ibin],
        norm * xi_save[ibin].real()
      );
    }
    fclose(save_fileptr);
  }

  delete[] r_save; delete[] xi_save;

  return corrfunc_out;
}

#endif  // TRIUMVIRATE_INCLUDE_TWOPT_HPP_INCLUDED_
