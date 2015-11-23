/*
 * Copyright 2014-2015, Max Planck Society.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors
 *    may be used to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

/* Created by Stephan Wenninger <stephan.wenninger@tuebingen.mpg.de> and
 * Edgar Klenske <edgar.klenske@tuebingen.mpg.de>
 */



#include "math_tools.h"
#include <Eigen/Dense>
#include <Eigen/Core>
#include <stdexcept>
#include <cmath>
#include <cstdint>

namespace math_tools {


Eigen::MatrixXd squareDistance(const Eigen::MatrixXd& a,
                               const Eigen::MatrixXd& b) {
  int aRows = a.rows();
  int aCols = a.cols();
  int bCols = b.cols();

  Eigen::MatrixXd am(aRows, aCols);    // mean-corrected a
  Eigen::MatrixXd bm(aRows, bCols);    // mean-corrected b
  // final result, aCols x bCols
  Eigen::MatrixXd result(aCols, bCols);

  Eigen::VectorXd mean(aRows);

  /*  If the two Matrices have the same address, it means the function was
      called from the overloaded version, and thus the mean only has to be
      computed once.
   */
  if (&a == &b) {  // Same address?
    mean = a.rowwise().mean();
    am = a.colwise() - mean;
    bm = am;
  } else {
    if (aRows != b.rows()) {
      throw std::runtime_error("Matrix dimension incorrect.");
    }

    mean = static_cast<double>(aCols) / (aCols + bCols) * a.rowwise().mean() +
      static_cast<double>(bCols) / (bCols + aCols) * b.rowwise().mean();

    // The mean of the two Matrices is subtracted beforehand, because the
    // squared error is independent of the mean and this makes the squares
    // smaller.
    am = a.colwise() - mean;
    bm = b.colwise() - mean;
  }

  Eigen::MatrixXd a_square =
    am.array().square().colwise()
    .sum().transpose().rowwise() .replicate(bCols);

  Eigen::MatrixXd b_square = bm.array().square().colwise().sum().colwise()
                             .replicate(aCols);

  Eigen::MatrixXd twoab = 2 * (am.transpose()) * bm;

  return (a_square.matrix() + b_square.matrix()) - twoab;
}

Eigen::MatrixXd squareDistance(const Eigen::MatrixXd& a) {
  return squareDistance(a, a);
}

Eigen::MatrixXd generate_random_sequence(int d, int n) {
  // x = randn(d,1); % starting sample
  Eigen::VectorXd x = math_tools::generate_normal_random_matrix(d, 1);

  Eigen::VectorXd t = math_tools::generate_normal_random_matrix(d, 1);

  return generate_random_sequence(n, x, t);
}

Eigen::MatrixXd generate_random_sequence(int n, Eigen::VectorXd x,
    Eigen::VectorXd t) {
// function X = GPanimation(d,n)
// % returns a matrix X of size [d,n], representing a grand circle on the
// % unit d-sphere in n steps, starting at a random location. Given a kernel
// % matrix K, this can be turned into a tour through the sample space, simply
// % by calling chol(K)’ * X;
// %
// % Philipp Hennig, September 2012


// r = sqrt(sum(x.^2));
  double r = std::sqrt(x.transpose() * x);

// x = x ./ r; % project onto sphere
  x = x / r;

// t = randn(d,1); % sample tangent direction

// t = t - (t'*x) * x; % orthogonalise by Gram-Schmidt.
  double tmp = t.adjoint() * x;
  t = t - tmp * x;

// t = t ./ sqrt(sum(t.^2)); % standardise
  t = t / std::sqrt(t.transpose() * t);

// s = linspace(0,2*pi,n+1); s = s(1:end-1); % space to span
  Eigen::VectorXd s(n + 1);
  s.setLinSpaced(n + 1, 0, 2 * M_PI);
  s.conservativeResize(s.rows() - 1);

// t = bsxfun(@times,s,t); % span linspace in direction of t
//     std::cout << (s.transpose().replicate(t.rows(),1)).format(OctaveFmt) <<
// std::endl;
//     std::cout << (t.replicate(1,s.rows())).format(OctaveFmt) <<
// std::endl;

  Eigen::MatrixXd T = s.transpose().replicate(t.rows(), 1)
                      .cwiseProduct(t.replicate(1, s.rows()));

// X = r.* exp_map(x,t); % project onto sphere, re-scale
  Eigen::MatrixXd X = r * exp_map(x, T);
// end
  return X;
}

Eigen::MatrixXd exp_map(const Eigen::VectorXd& mu, const Eigen::MatrixXd& E) {
// D = size(E,1);
  int D = E.rows();

// theta = sqrt(sum((E.^2)));
  Eigen::MatrixXd theta = E.array().pow(2).colwise().sum().sqrt();

// M = mu * cos(theta) + E .* repmat(sin(theta)./theta, D, 1);
  Eigen::MatrixXd M = mu * theta.array().cos().matrix() +
                      E.cwiseProduct(
                        (theta.array().sin() / theta.array())
                        .matrix().replicate(D, 1));

// if (any (abs (theta) <= 1e-7))
// for a = find (abs (theta) <= 1e-7)
// M (:, a) = mu;
// end % for
// end % if
  for (int i = 0; i < theta.cols(); i++) {
    if (theta(0, i) < MINIMAL_THETA) {
      M.col(i) = mu;
    }
  }

// end % function
  return M;
}

Eigen::MatrixXd generate_uniform_random_matrix_0_1(
    const size_t n,
    const size_t m) {
  Eigen::MatrixXd result = Eigen::MatrixXd(n, m);
  result.setRandom();
  Eigen::MatrixXd temp = result.array() + 1;
  result = temp / 2.0;
  result = result.array().max(1e-10);
  result = result.array().min(1.0);
  return result;
}

Eigen::MatrixXd box_muller(const Eigen::VectorXd &vRand) {
  size_t n = vRand.rows();
  size_t m = n / 2;

  Eigen::ArrayXd rand1 = vRand.head(m);
  Eigen::ArrayXd rand2 = vRand.tail(m);

  /* Implemented according to
   * http://en.wikipedia.org/wiki/Box%E2%80%93Muller_transform
   */

  rand1 = rand1.max(1e-10);
  rand1 = rand1.min(1.0);

  rand1 = -2 * rand1.log();
  rand1 = rand1.sqrt();

  rand2 = rand2 * 2 * M_PI;

  Eigen::MatrixXd result(2 * m, 1);
  Eigen::MatrixXd res1 = (rand1 * rand2.cos()).matrix();
  Eigen::MatrixXd res2 = (rand1 * rand2.sin()).matrix();
  result << res1, res2;

  return result;
}

Eigen::MatrixXd generate_normal_random_matrix(
    const size_t n,
    const size_t m) {
  // if n*m is odd, we need one random number extra!
  // therefore, we have to round up here.
  size_t N = static_cast<size_t>(std::ceil(n * m / 2.0));

  Eigen::MatrixXd result(2 * N, 1);
  // push random samples through the Box-Muller transform
  result = box_muller(generate_uniform_random_matrix_0_1(2 * N, 1));
  result.conservativeResize(n, m);
  return result;
}

double generate_normal_random_double() {
  Eigen::MatrixXd randomMatrix = generate_normal_random_matrix(1, 1);
  return randomMatrix(0,0);
}

double generate_uniform_random_double() {
  Eigen::MatrixXd randomMatrix = generate_uniform_random_matrix_0_1(1, 1);
  return randomMatrix(0,0);
}

std::pair<Eigen::VectorXd, Eigen::VectorXd> compute_spectrum(Eigen::VectorXd &data, int N) {

  int N_data = data.rows();

  if(N<N_data) {
    N = N_data;
  }
  N = std::pow(2,std::ceil(std::log(N)/std::log(2)));

  Eigen::VectorXd padded_data = Eigen::VectorXd::Zero(N);
  padded_data.head(N_data) = data;

  Eigen::VectorXcd result = ditfft2(padded_data, N, 1);

  int low_index = std::round(static_cast<double>(N)/static_cast<double>(N_data));

  Eigen::VectorXd spectrum = result.segment(low_index,N/2 - low_index + 1).array().abs().pow(2);

  Eigen::VectorXd frequencies = Eigen::VectorXd::LinSpaced(N/2 - low_index + 1, low_index, N/2);
  frequencies /= N;

  return std::make_pair(spectrum, frequencies);
}

Eigen::VectorXcd ditfft2(Eigen::VectorXd data, int N, int S) {

  Eigen::VectorXcd result(N);

  if(N==1) {
    result(0) = data(0);
  } else {
    result.head(N/2) = ditfft2(data, N/2, 2*S);
    result.tail(N/2) = ditfft2(data.segment(S,data.rows()-S), N/2, 2*S);
    for(int k=0; k<N/2; ++k) {
      std::complex<double> t = result(k);
      std::complex<double> i(0, 1);
      std::complex<double> twid = std::exp(-2*M_PI*i*static_cast<double>(k)/static_cast<double>(N));
      result(k) = t + twid*result(k+N/2);
      result(k+N/2) = t - twid*result(k+N/2);
    }
  }

  return result;
}

}  // namespace math_tools





