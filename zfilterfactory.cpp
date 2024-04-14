/*
*  zfilterfactory.cpp
*  PHD Guiding
*
*  Created by Ken Self
*  Copyright (c) 2018 Ken Self
*  All rights reserved.
*
*  This source code is distributed under the following "BSD" license
*  Redistribution and use in source and binary forms, with or without
*  modification, are permitted provided that the following conditions are met:
*    Redistributions of source code must retain the above copyright notice,
*     this list of conditions and the following disclaimer.
*    Redistributions in binary form must reproduce the above copyright notice,
*     this list of conditions and the following disclaimer in the
*     documentation and/or other materials provided with the distribution.
*    Neither the name of openphdguiding.org nor the names of its
*     contributors may be used to endorse or promote products derived from
*     this software without specific prior written permission.
*
*  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
*  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
*  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
*  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
*  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
*  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
*  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
*  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
*  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
*  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
*  POSSIBILITY OF SUCH DAMAGE.
*
*/

/* Based on mkfilter by A.J. Fisher, University of York    September 1992
https://www-users.cs.york.ac.uk/~fisher/mkfilter/
<fisher@minster.york.ac.uk>
*/

#include "phd.h"

#include "zfilterfactory.h"

#include <stdio.h>
#include <math.h>
#include <string.h>

#include "zfilterfactory.h"

ZFilterFactory::ZFilterFactory(FILTER_DESIGN f, int o, double p, bool mzt )
{
    bessel_poles = {
    /* table produced by /usr/fisher/bessel --  N.B. only one member of each C.Conj. pair is listed */
        std::complex<double>( -1.00000000000e+00, 0.00000000000e+00),
        std::complex<double>( -1.10160133059e+00, 6.36009824757e-01),
        std::complex<double>( -1.32267579991e+00, 0.00000000000e+00),
        std::complex<double>( -1.04740916101e+00, 9.99264436281e-01),
        std::complex<double>( -1.37006783055e+00, 4.10249717494e-01),
        std::complex<double>( -9.95208764350e-01, 1.25710573945e+00),
        std::complex<double>( -1.50231627145e+00, 0.00000000000e+00),
        std::complex<double>( -1.38087732586e+00, 7.17909587627e-01),
        std::complex<double>( -9.57676548563e-01, 1.47112432073e+00),
        std::complex<double>( -1.57149040362e+00, 3.20896374221e-01),
        std::complex<double>( -1.38185809760e+00, 9.71471890712e-01),
        std::complex<double>( -9.30656522947e-01, 1.66186326894e+00),
        std::complex<double>( -1.68436817927e+00, 0.00000000000e+00),
        std::complex<double>( -1.61203876622e+00, 5.89244506931e-01),
        std::complex<double>( -1.37890321680e+00, 1.19156677780e+00),
        std::complex<double>( -9.09867780623e-01, 1.83645135304e+00),
        std::complex<double>( -1.75740840040e+00, 2.72867575103e-01),
        std::complex<double>( -1.63693941813e+00, 8.22795625139e-01),
        std::complex<double>( -1.37384121764e+00, 1.38835657588e+00),
        std::complex<double>( -8.92869718847e-01, 1.99832584364e+00),
        std::complex<double>( -1.85660050123e+00, 0.00000000000e+00),
        std::complex<double>( -1.80717053496e+00, 5.12383730575e-01),
        std::complex<double>( -1.65239648458e+00, 1.03138956698e+00),
        std::complex<double>( -1.36758830979e+00, 1.56773371224e+00),
        std::complex<double>( -8.78399276161e-01, 2.14980052431e+00),
        std::complex<double>( -1.92761969145e+00, 2.41623471082e-01),
        std::complex<double>( -1.84219624443e+00, 7.27257597722e-01),
        std::complex<double>( -1.66181024140e+00, 1.22110021857e+00),
        std::complex<double>( -1.36069227838e+00, 1.73350574267e+00),
        std::complex<double>( -8.65756901707e-01, 2.29260483098e+00),
    };
    if (o <= 0)
    {
        throw ERROR_INFO("invalid filter order");
    }
    if (p < 2.0)
    {
        throw ERROR_INFO("invalid corner period multiplier");
    }
    filt = f;
    m_order = o;
    raw_alpha2 = raw_alpha1 = 1.0 / p;
    isMzt = (f == BESSEL) ? mzt : false; // mzt only applies to BESSEL filter
    spoles.clear();
    szeros.clear();

    splane();
    prewarp();
    normalize();
    zplane();
    expandpoly();
}

void ZFilterFactory::splane() // compute S-plane poles for prototype LP filter
{
// Bessel filter
//
    if (filt == BESSEL)                           // Bessel filter
    {
        int p = (m_order * m_order) / 4;                // ptr into table
        if (m_order & 1)                              // If order is odd
            setpole(bessel_poles[p++]);
        for (int i = 0; i < m_order / 2; i++)
        {
            setpole(bessel_poles[p]);
            setpole(std::conj(bessel_poles[p]));
            p++;
        }
    }
    if (filt == BUTTERWORTH || filt == CHEBYCHEV)                // Butterworth filter
    {
        for (int i = 0; i < 2 * m_order; i++)
        {
            double theta = (m_order & 1) ? (i * M_PI) / m_order : ((i + 0.5) * M_PI) / m_order;
            setpole(std::polar(1.0,theta));
        }
    }
    if (filt == CHEBYCHEV)                           // modify for Chebyshev (p. 136 DeFatta et al.)
    {
        if (chripple >= 0.0)
        {
            fprintf(stderr, "mkfilter: Chebyshev ripple is %g dB; must be .lt. 0.0\n", chripple);
            exit(1);
        }
        double rip = pow(10.0, -chripple / 10.0);
        double eps = sqrt(rip - 1.0);
        double y = asinh(1.0 / eps) / (double) m_order;
        for (int i = 0; i < spoles.size(); i++)
        {
            spoles[i].real(spoles[i].real()*sinh(y));
            spoles[i].imag(spoles[i].imag()*cosh(y));
        }
    }
}

void ZFilterFactory::prewarp()
{
    /* for bilinear transform, perform pre-warp on alpha values */
    if (isMzt)
    { //**Dont prewarp; or use z-transform
        warped_alpha1 = raw_alpha1;
        warped_alpha2 = raw_alpha2;
    }
    else
    {
        warped_alpha1 = tan(M_PI * raw_alpha1) / M_PI;
        warped_alpha2 = tan(M_PI * raw_alpha2) / M_PI;
    }
}

void ZFilterFactory::normalize() /* called for trad, not for -Re or -Pi */
{
    double w1 = TWOPI * warped_alpha1;
    for (int i = 0; i < spoles.size(); i++)
        spoles[i] = spoles[i] * w1;
    szeros.clear();
}

void ZFilterFactory::zplane()
{
// given S-plane poles & zeros, compute Z-plane poles & zeros
// using bilinear transform or matched z-transform
    int i;
    zpoles.clear();
    zzeros.clear();
    if(!isMzt)
    {
        for (i = 0; i < spoles.size(); i++)
            zpoles.push_back(bilinear(spoles[i]));
        for (i = 0; i < szeros.size(); i++)
            zzeros.push_back(bilinear(szeros[i]));
        while (zzeros.size() < zpoles.size())
            zzeros.push_back(-1.0);
    }
    else
    {
        for (i = 0; i < spoles.size(); i++)
            zpoles.push_back(std::exp(spoles[i]));
        for (i = 0; i < szeros.size(); i++)
            zzeros.push_back(std::exp(szeros[i]));
    }
}

void ZFilterFactory::expandpoly() // given Z-plane poles & zeros, compute top & bot polynomials in Z, and then recurrence relation
{
    std::vector<std::complex<double>> topcoeffs, botcoeffs;
    int i;
    expand(zzeros, topcoeffs);
    expand(zpoles, botcoeffs);

    double theta = TWOPI * 0.5 * (raw_alpha1 + raw_alpha2); /* "jwT" for centre freq. */
    const std::complex<double> z_one(1.0, 0.0);
    const std::complex<double> z_minusone(-1.0, 0.0);
    const std::complex<double> z_theta = std::polar(1.0, theta);

    xcoeffs.clear();
    ycoeffs.clear();
    dc_gain = eval(topcoeffs, z_one) / eval(botcoeffs, z_one);
    fc_gain = eval(topcoeffs, z_theta) / eval(botcoeffs, z_theta);
    hf_gain = eval(topcoeffs, z_minusone) / eval(botcoeffs, z_minusone);
    for (i = topcoeffs.size()-1; i >= 0; i--)
        xcoeffs.push_back( +(topcoeffs[i].real() / botcoeffs.back().real()) );
    for (i = botcoeffs.size()-1; i >= 0; i--)
        ycoeffs.push_back( -(botcoeffs[i].real() / botcoeffs.back().real()) );
}

void ZFilterFactory::expand(const std::vector<std::complex<double>>& pz, std::vector<std::complex<double>>& coeffs)
{
    // compute product of poles or zeros as a polynomial of z
    int i;
    coeffs.clear();
    coeffs.push_back(1.0);
    for (i = 0; i < pz.size(); i++)
        coeffs.push_back(0.0);

// coeffs now has 1 more element than pz
    for (i = 0; i < pz.size(); i++)
        multin(pz[i], coeffs);

// check computed coeffs of z^k are all real
    for (i = 0; i < coeffs.size(); i++)
    {
        if (fabs(coeffs[i].imag()) > EPS)
        {
            fprintf(stderr, "mkfilter: coeff of z^%d is not real; poles/zeros are not complex conjugates\n", i);
            exit(1);
        }
    }
}

void ZFilterFactory::multin(const std::complex<double>& w, std::vector<std::complex<double>>& coeffs)
{
    /* multiply factor (z-w) into coeffs */
    std::complex<double> nw = -w;
    for (int i = coeffs.size()-1; i >= 1; i--)
        coeffs[i] = (nw * coeffs[i]) + coeffs[i - 1];
    coeffs[0] = nw * coeffs[0];
}

std::complex<double> ZFilterFactory::eval(const std::vector<std::complex<double>>& coeffs, const std::complex<double>& z)
{ /* evaluate polynomial in z, substituting for z */
    std::complex<double> sum = std::complex<double>(0.0,0.0);
    for (int i = coeffs.size()-1; i >= 0; i--)
        sum = (sum * z) + coeffs[i];
    return sum;
}
