#ifndef TRIUMVIRATE_INCLUDE_TWOPT_HPP_INCLUDED_
#define TRIUMVIRATE_INCLUDE_TWOPT_HPP_INCLUDED_

#include "field.hpp"

/**
 * Calculate power spectrum from catalogues and save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param particles_rand (Random-source) particle container.
 * @param los_data (Data-source) particle lines of sight.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_powspec(
  ParticleCatalogue& particles_data, ParticleCatalogue& particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* kbin,
  double survey_vol_norm
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring power spectrum from data and random catalogues.\n"
    );

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopoles of the Fourier--harmonic transform of the density
  /// fluctuation, i.e. δn_00.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_fluctuation(
    particles_data, particles_rand,
    los_data, los_rand,
    alpha,
    0, 0
  );
  dn_00.fourier_transform();

  /// Compute power spectrum with shot noise.
  double* k_save = new double[params.num_kbin];
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  std::complex<double>* sn_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    k_save[i] = 0.;
    pk_save[i] = 0.;
    sn_save[i] = 0.;
  }

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
      // NOBUG: naming convention overriden
    dn_LM.compute_ylm_wgtd_fluctuation(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );
    dn_LM.fourier_transform();

    /// Compute shot noise.
    Pseudo2ptStats<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );

    /// Calculate equivalent to (-1)^m_1 δ_{m_1, -M} which, after being
    /// summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
        * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.compute_2pt_stats_in_fourier(
        dn_LM, dn_00,
        shotnoise,
        kbin,
        params.ell1, m1_
      );

      for (int i = 0; i < params.num_kbin; i++) {
        pk_save[i] += coupling * stats.pk[i];
        sn_save[i] += coupling * stats.sn[i];
      }

      if (M_ == 0 && m1_ == 0) {
        for (int i = 0; i < params.num_kbin; i++) {
          k_save[i] += stats.k[i];
        }
      }
    }

    durationInSec = double(clock() - timeStart);
    if (currTask == 0) {
      printf(
        "[Status] :: Computed power spectrum term of order `M = %d` "
        "(%.3f seconds elapsed).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = survey_vol_norm
    / particles_data.wtotal / particles_data.wtotal;

  char buf[1024];
  sprintf(
    buf, "%s/pk%d%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.5f \t %.7e \t %.7e\n",
      kbin[i], k_save[i], norm * pk_save[i].real(), norm * sn_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation function from catalogues
 * and save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param particles_rand (Random-source) particle container.
 * @param los_data (Data-source) particle lines of sight.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param alpha Alpha ratio.
 * @param rbin Separation bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_corrfunc(
  ParticleCatalogue& particles_data, ParticleCatalogue& particles_rand,
  LineOfSight* los_data, LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* rbin,
  double survey_vol_norm
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring two-point correlation function "
      "from data and random catalogues.\n"
    );

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlation function measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of the density
  /// fluctuation.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_fluctuation(
    particles_data, particles_rand,
    los_data, los_rand,
    alpha,
    0, 0
  );
  dn_00.fourier_transform();

  /// Compute 2PCF.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
      // NOBUG: naming convention overriden
    dn_LM.compute_ylm_wgtd_fluctuation(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );
    dn_LM.fourier_transform();

    /// Compute shot noise.
    Pseudo2ptStats<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_data, particles_rand,
      los_data, los_rand,
      alpha,
      params.ELL, M_
    );

    /// Calculate equivalent to (-1)^m_1 δ_{m_1, -M} which, after being
    /// summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
        * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
        * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.compute_2pt_stats_in_config(
        dn_LM, dn_00,
        shotnoise,
        rbin,
        params.ell1, m1_
      );

      for (int i = 0; i < params.num_rbin; i++) {
        xi_save[i] += coupling * stats.xi[i];
      }
    }

    durationInSec = double(clock() - timeStart);
    if (currTask == 0) {
      printf(
        "[Status] :: Computed two-point correlation function term with "
        "order `M = %d` (%.3f seconds elapsed).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = survey_vol_norm
    / particles_data.wtotal / particles_data.wtotal;

  char buf[1024];
  sprintf(
    buf, "%s/xi%d%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum window from random catalogues
 * and save the results.
 *
 * @param particles_rand (Random-source) particle container.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_powspec_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* kbin,
  double survey_vol_norm
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring power spectrum window from random catalogues.\n"
    );

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlator measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of
  /// the mean density.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.fourier_transform();

  /// Initialise output power spectrum window.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }
  std::cout << "Current memory usage: " << bytesMem << " bytesMem." << std::endl;

  /// Compute shot noise.
  Pseudo2ptStats<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = stats.calc_ylm_wgtd_shotnoise_for_powspec(
    particles_rand, los_rand, alpha, params.ELL, 0
  );
  std::cout << "Current memory usage: " << bytesMem << " bytesMem." << std::endl;

  /// Compute power spectrum window.
  stats.compute_2pt_stats_in_fourier(
    dn_00, dn_00,
    shotnoise,
    kbin,
    params.ELL, 0
  );  // `ell1` or `ELL` as  equivalent

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += stats.pk[i];
  }
  std::cout << "Current memory usage: " << bytesMem << " bytesMem." << std::endl;

  /// Normalise and then save the output.
  double norm = survey_vol_norm
    / particles_rand.wtotal / particles_rand.wtotal;
  norm /= alpha * alpha;
  norm /= params.volume;  // NOTE: volume normalisation is essential

  char buf[1024];
  sprintf(
    buf, "%s/pk%d_window%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  if (0) {
  } else if (currTask == 0) {
    printf(
      "[Info] :: Power spectrum window in the lowest wavenumber bin: %.2f.",
      norm * pk_save[0].real()
    );
  }

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation function window from random catalogues
 * and save the results.
 *
 * @param particles_rand (Random-source) particle container.
 * @param los_rand (Random-source) particle lines of sight.
 * @param params Parameter set.
 * @param alpha Alpha ratio.
 * @param kbin Wavenumber bins.
 * @param survey_vol_norm Survey volume normalisation constant.
 * @returns Exit status.
 */
int calc_corrfunc_window(
  ParticleCatalogue& particles_rand,
  LineOfSight* los_rand,
  ParameterSet& params,
  double alpha,
  double* rbin,
  double survey_vol_norm
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring two-point correlation function window "
			"from random catalogues.\n"
		);

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlator measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Compute monopole of the Fourier--harmonic transform of
  /// the mean density.
  PseudoDensityField<ParticleCatalogue> dn_00(params);
  dn_00.compute_ylm_wgtd_density(particles_rand, los_rand, alpha, 0, 0);
  dn_00.fourier_transform();

  /// Initialise output 2PCF.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  /// Compute 2PCF.
  for (int M_ = - params.ELL; M_ <= params.ELL; M_++) {
    /// Compute Fourier--harmonic transform of the density fluctuation.
    PseudoDensityField<ParticleCatalogue> dn_LM(params);
      // NOBUG: naming convention overriden
    dn_LM.compute_ylm_wgtd_density(
      particles_rand, los_rand, alpha, params.ELL, M_
    );
    dn_LM.fourier_transform();

    /// Compute shot noise.
    Pseudo2ptStats<ParticleCatalogue> stats(params);
    std::complex<double> shotnoise = stats.calc_ylm_wgtd_shotnoise_for_powspec(
      particles_rand, los_rand, alpha, params.ELL, M_
    );

    /// Calculate equivalent to (-1)^m_1 δ_{m_1, -M} which, after being
    /// summed over m_1, agrees with Hand et al. (2017) [1704.02357].
    for (int m1_ = - params.ell1; m1_ <= params.ell1; m1_++) {
      double coupling = (2*params.ELL + 1) * (2*params.ell1 + 1)
          * wigner_3j(params.ell1, 0, params.ELL, 0, 0, 0)
          * wigner_3j(params.ell1, 0, params.ELL, m1_, 0, M_);
      if (fabs(coupling) < 1.e-10) {
        continue;
      }

      stats.compute_2pt_stats_in_config(
        dn_LM, dn_00,
        shotnoise,
        rbin,
        params.ell1, m1_
      );

      for (int i = 0; i < params.num_rbin; i++) {
        xi_save[i] += coupling * stats.xi[i];
      }
    }

    durationInSec = double(clock() - timeStart);
    if (currTask == 0) {
      printf(
        "[Status] :: Computed two-point correlation function window term with "
        "order `M = %d` (%.3f seconds elapsed).\n",
        M_, durationInSec / CLOCKS_PER_SEC
      );
    }
  }

  /// Normalise and then save the output.
  double norm = survey_vol_norm
    / particles_rand.wtotal / particles_rand.wtotal;
  norm /= alpha * alpha;

  char buf[1024];
  sprintf(
    buf, "%s/xi%d_window%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum in a periodic box and save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_powspec_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* kbin
) {
  if (currTask == 0) {
    printf("[Info] :: Measuring power spectrum in a periodic box.\n");

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  PseudoDensityField<ParticleCatalogue> dn(params);
  dn.compute_unweighted_fluctuation_insitu(
		particles_data, params.volume
	);
  dn.fourier_transform();

  /// Initialise output power spectrum.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }

  /// Compute shot noise.
  Pseudo2ptStats<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = double(particles_data.ntotal);

  /// Compute power spectrum.
  stats.compute_2pt_stats_in_fourier(
    dn, dn,
    shotnoise,
    kbin,
    params.ELL, 0
  );

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += double(2*params.ELL + 1) * stats.pk[i];
  }

  durationInSec = double(clock() - timeStart);
  if (currTask == 0) {
    printf(
      "[Status] :: Computed power spectrum terms "
      "(... %.3f seconds elapsed).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.ntotal) / double(particles_data.ntotal);

  char buf[1024];
  sprintf(
    buf, "%s/pk%d%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

/**
 * Calculate two-point correlation function in a periodic box
 * and save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_corrfunc_in_box(
  ParticleCatalogue& particles_data,
  ParameterSet& params,
  double* rbin
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring two-point correlation function "
      "in a periodic box.\n"
    );

    if (!(params.ell1 == params.ELL) && (params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for two-point correlation function measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  PseudoDensityField<ParticleCatalogue> dn(params);
  dn.compute_unweighted_fluctuation_insitu(
		particles_data, params.volume
	);
  dn.fourier_transform();

  /// Initialise output 2PCF.
  std::complex<double>* xi_save = new std::complex<double>[params.num_rbin];
  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] = 0.;
  }

  /// Compute shot noise.
  Pseudo2ptStats<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise = double(particles_data.ntotal);

  /// Compute 2PCF.
  stats.compute_2pt_stats_in_config(
    dn, dn,
    shotnoise,
    rbin,
    params.ELL, 0
  );

  for (int i = 0; i < params.num_rbin; i++) {
    xi_save[i] += double(2 * params.ELL + 1) * stats.xi[i];
      // NOTE: `double` needed here as complex multiplication is defined
      // as a template unlike normal floats
  }

  durationInSec = double(clock() - timeStart);
  if (currTask == 0) {
    printf(
      "[Status] :: Computed two-point correlation function terms "
      "(... %.3f seconds elapsed).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.ntotal) / double(particles_data.ntotal);

  char buf[1024];
  sprintf(
    buf, "%s/xi%d%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_rbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", rbin[i], norm * xi_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] xi_save;

  return 0;
}

/**
 * Calculate power spectrum in a periodic box for reconstruction
 * and save the results.
 *
 * @param particles_data (Data-source) particle container.
 * @param particles_rand (Random-source) particle container.
 * @param params Parameter set.
 * @param kbin Wavenumber bins.
 * @returns Exit status.
 */
int calc_powspec_in_box_for_recon(
  ParticleCatalogue& particles_data,
  ParticleCatalogue& particles_rand,
  ParameterSet& params,
  double alpha,
  double* kbin
) {
  if (currTask == 0) {
    printf(
      "[Info] :: Measuring power spectrum in a periodic box "
      "for reconstruction.\n"
    );

    if (!(params.ell1 == params.ELL && params.ell2 == 0)) {
      printf(
        "[Error] :: Disallowed multipole degree combination "
        "for power spectrum measurements. "
        "Please set `ell1 = ELL` and `ell2 = 0`.\n"
      );
      exit(1);
    }
  }

  /// Fourier transform the density field.
  PseudoDensityField<ParticleCatalogue> dn(params);
  dn.compute_unweighted_fluctuation(
    particles_data, particles_rand, alpha
  );
  dn.fourier_transform();

  /// Initialise output power spectrum.
  std::complex<double>* pk_save = new std::complex<double>[params.num_kbin];
  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] = 0.;
  }

  /// Compute shot noise.
  Pseudo2ptStats<ParticleCatalogue> stats(params);
  std::complex<double> shotnoise =
    stats.calc_unweighted_shotnoise_for_powspec(
      particles_data, particles_rand, alpha
    );

  /// Compute power spectrum.
  stats.compute_2pt_stats_in_fourier(
    dn, dn,
    shotnoise,
    kbin,
    params.ELL, 0
  );

  for (int i = 0; i < params.num_kbin; i++) {
    pk_save[i] += double(2*params.ELL + 1) * stats.pk[i];
      // NOTE: `double` needed here as complex multiplication is defined
      // as a template unlike normal floats
  }

  durationInSec = double(clock() - timeStart);
  if (currTask == 0) {
    printf(
      "[Status] :: Computed power spectrum terms "
      "(... %.3f seconds elapsed).\n",
      durationInSec / CLOCKS_PER_SEC
    );
  }

  /// Normalise and then save the output.
  double norm = params.volume
    / double(particles_data.ntotal) / double(particles_data.ntotal);

  char buf[1024];
  sprintf(
    buf, "%s/pk%d%s",
    params.measurement_dir.c_str(), params.ELL, params.output_tag.c_str()
  );

  FILE* saved_file_ptr;
  saved_file_ptr = fopen(buf, "w");
  for (int i = 0; i < params.num_kbin; i++) {
    fprintf(
      saved_file_ptr, "%.5f \t %.7e\n", kbin[i], norm * pk_save[i].real()
    );
  }
  fclose(saved_file_ptr);

  delete[] pk_save;

  return 0;
}

#endif