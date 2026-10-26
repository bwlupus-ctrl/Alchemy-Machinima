/**
 * @file sensorUtilF.glsl
 * @brief Pure sensor functions. Binding equations: doc/iso_sensor/SENSOR_MODEL.md.
 * No samplers, implicit clock, exposure, processing, or pipeline integration.
 * $LicenseInfo:firstyear=2026&license=viewerlgpl$
 * Alchemy-Machinima fork
 * $/LicenseInfo$
 */

// Layout mirrors ALSensorModel::Uniforms. mat3 columns use std140's vec4 stride.
struct SensorParameters
{
    vec4 physics; // FW, sigma_r, analog DN/e-, black pedestal DN
    vec4 scale; // white DN, K electrons/linear, g_N, highlight knee fraction
    vec4 controls; // noise amount, chroma amount, total ISO gain, green read RMS
    mat3 cameraToWorking;
    mat3 workingToCamera;
    uvec4 clock; // capture-relative frame low/high, take seed, presentation stream
    mat3 readCameraBasis; // precomputed camera-space C*orthonormal luma/chroma rotation
    uvec4 randomContext; // capture tuple prefix, computed with identical integer math on CPU
};

uint sensorHash(uint x)
{
    x ^= x >> 16u;
    x *= 0x7feb352du;
    x ^= x >> 15u;
    x *= 0x846ca68bu;
    return x ^ (x >> 16u);
}

uint sensorContext(SensorParameters p)
{
    return p.randomContext.x;
}

uint sensorKey(ivec2 pixel, SensorParameters p, uint domain)
{
    // Avalanche EACH component before combining the next. In particular,
    // frame XOR domain must not permute the two green-site streams.
    // Put the uniform prefix first so all lattice neighbours share its work.
    uint spatial = sensorHash(uint(pixel.x) ^ 0x85ebca6bu);
    spatial = sensorHash(spatial ^ uint(pixel.y));
    return sensorHash(spatial ^ sensorHash(sensorContext(p) ^ domain));
}

float sensorUniform(uint key)
{
    // key is already avalanched by sensorKey (or explicitly sensorHash).
    // Open interval, 23 random bits; avoids log(0) and rounded-to-one uniforms.
    return (float(key >> 9u) + 0.5) * (1.0 / 8388608.0);
}

vec2 sensorNormalPair(uint key)
{
    float radius = sqrt(-2.0 * log(sensorUniform(key)));
    float theta = 6.283185307179586 * sensorUniform(sensorHash(key ^ 0x68bc21ebu));
    return radius * vec2(cos(theta), sin(theta));
}

bool sensorPoissonBlock(vec4 lambda, vec4 u, vec4 inverseK,
                        inout vec4 mass, inout vec4 cdf, inout vec4 value)
{
    // Evaluate four PMF terms from the same starting mass. The constant
    // reciprocal products fold, and independent powers shorten the dependency
    // chain without changing the inverse-CDF distribution.
    vec4 square = lambda*lambda;
    vec4 cube = square*lambda;
    vec4 fourth = square*square;
    vec4 m1 = mass*lambda*inverseK.x;
    vec4 m2 = mass*square*(inverseK.x*inverseK.y);
    vec4 m3 = mass*cube*(inverseK.x*inverseK.y*inverseK.z);
    vec4 m4 = mass*fourth*(inverseK.x*inverseK.y*inverseK.z*inverseK.w);
    vec4 c1 = cdf+m1;
    vec4 c2 = c1+m2;
    vec4 c3 = c2+m3;
    value += vec4(greaterThan(u,cdf)) + vec4(greaterThan(u,c1)) +
             vec4(greaterThan(u,c2)) + vec4(greaterThan(u,c3));
    cdf = c3+m4;
    mass = m4;
    return all(lessThanEqual(u,cdf));
}

vec4 sensorPoissonCDF(vec4 lambda, vec4 u)
{
    vec4 mass = exp(-lambda);
    vec4 cdf = mass;
    vec4 value = vec4(0.0);
    // Return min(Poisson(lambda), 64). For lambda<20, Chernoff gives
    // P(X>=65) <= exp(-20)*(e*20/65)^65 < 2e-14. Float CDF error is
    // separate (~a few ulps), tested by histogram GOF; no unbounded rejection.
    // Explicitly unroll the bounded 64-step recurrence in blocks of four.
    // Constant reciprocals fold at compile time, avoiding a per-iteration
    // division and reducing loop-control cost in large near-20 flat patches.
    if (all(lessThanEqual(u,cdf))) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(1,2,3,4),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(5,6,7,8),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(9,10,11,12),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(13,14,15,16),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(17,18,19,20),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(21,22,23,24),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(25,26,27,28),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(29,30,31,32),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(33,34,35,36),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(37,38,39,40),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(41,42,43,44),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(45,46,47,48),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(49,50,51,52),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(53,54,55,56),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(57,58,59,60),mass,cdf,value)) return value;
    if (sensorPoissonBlock(lambda,u,1.0/vec4(61,62,63,64),mass,cdf,value)) return value;
    return value;
}

float sensorPoissonUpper(float lambda, float u)
{
    // Sum the small survival probability, never 1-CDF. Start above the
    // representable tail; this adaptive start also avoids PMF underflow for
    // tiny lambda. The omitted tail is <2e-14 throughout 0<lambda<20.
    if (lambda <= 1.0-u) return 0.0;
    int top = lambda < 1e-4 ? 3 : (lambda < 0.01 ? 6 :
        min(64, int(ceil(lambda + 10.0*sqrt(lambda) + 6.0))));
    float mass = exp(-lambda);
    for (int j=1; j<=64; ++j)
    {
        if (j > top) break;
        mass *= lambda/float(j);
    }
    float survival = mass;
    float target = 1.0-u;
    for (int j=top; j>=1; --j)
    {
        if (survival > target) return float(j);
        mass *= float(j)/lambda;
        survival += mass;
    }
    return 0.0;
}

vec4 sensorPoissonQuantile(vec4 lambda, vec4 u)
{
    // Tail lanes are overwritten below; exclude them from the lower-CDF exit
    // condition without changing any recurrence arithmetic for other lanes.
    vec4 value = sensorPoissonCDF(lambda,mix(u,vec4(0.0),greaterThan(u,vec4(0.9999))));
    if (u.x > 0.9999) value.x = sensorPoissonUpper(lambda.x,u.x);
    if (u.y > 0.9999) value.y = sensorPoissonUpper(lambda.y,u.y);
    if (u.z > 0.9999) value.z = sensorPoissonUpper(lambda.z,u.z);
    if (u.w > 0.9999) value.w = sensorPoissonUpper(lambda.w,u.w);
    return value;
}

vec4 sensorPoisson4(vec4 lambda, uvec4 key)
{
    vec4 u = vec4(sensorUniform(key.x),sensorUniform(key.y),sensorUniform(key.z),sensorUniform(key.w));
    return sensorPoissonQuantile(lambda,u);
}

float sensorPoisson(float lambda, uint key)
{
    return sensorPoisson4(vec4(lambda),uvec4(key)).x;
}

float sensorShot(float lambda, uint key)
{
    if (lambda < 20.0) return sensorPoisson(lambda, key);
    return lambda + sqrt(lambda) * sensorNormalPair(key).x;
}

mat3 sensorCovariance(mat3 m, vec3 variance)
{
    return m * mat3(variance.x, 0, 0, 0, variance.y, 0, 0, 0, variance.z) * transpose(m);
}

mat3 sensorCholesky(mat3 s)
{
    float c00 = sqrt(max(s[0][0], 0.0));
    float c10 = c00 > 0.0 ? s[0][1] / c00 : 0.0;
    float c20 = c00 > 0.0 ? s[0][2] / c00 : 0.0;
    float c11 = sqrt(max(s[1][1] - c10*c10, 0.0));
    float c21 = c11 > 0.0 ? (s[1][2] - c20*c10) / c11 : 0.0;
    float c22 = sqrt(max(s[2][2] - c20*c20 - c21*c21, 0.0));
    return mat3(c00, c10, c20, 0, c11, c21, 0, 0, c22);
}

vec2 sensorChroma(ivec2 pixel, SensorParameters p, uint domain)
{
    // Symmetric sub-cell weights plus a capture-relative random pixel origin
    // remove fixed parity correlations across frames (including lag two).
    uint context = sensorContext(p);
    vec2 origin = 0.25 + 0.5*vec2(context & 1u,(context >> 1u) & 1u);
    vec2 position = vec2(pixel) * 0.5 + origin;
    ivec2 cell = ivec2(floor(position));
    vec2 f = fract(position);
    vec4 w = vec4((1.0-f.x)*(1.0-f.y), f.x*(1.0-f.y), (1.0-f.x)*f.y, f.x*f.y);
    vec2 value = w.x * sensorNormalPair(sensorKey(cell, p, domain));
    value += w.y * sensorNormalPair(sensorKey(cell + ivec2(1,0), p, domain));
    value += w.z * sensorNormalPair(sensorKey(cell + ivec2(0,1), p, domain));
    value += w.w * sensorNormalPair(sensorKey(cell + ivec2(1,1), p, domain));
    // Quarter-cell phases all have sum(w*w)=25/64. Keep the general path
    // for integer coordinates so large that float conversion loses the phase.
    bool quarterPhase = all(equal(abs(f-0.5),vec2(0.25)));
    return value / (quarterPhase ? 0.625 : sqrt(dot(w, w)));
}

vec3 sensorGaussian(mat3 c, ivec2 pixel, SensorParameters p, uint domain, out float companion)
{
    // Rotate the whitened variables: C^T*luma selects the full-res axis.
    // Two independent half-res normals span its orthogonal complement.
    // Rotation preserves I, hence covariance remains C*C^T, with pure luma
    // full-res even for unequal channel variances / nontrivial colour matrices.
    vec3 axis = transpose(c) * vec3(0.2126, 0.7152, 0.0722);
    float axisLength = length(axis);
    companion = 0.0;
    if (axisLength < 1e-12) return vec3(0.0);
    axis /= axisLength;
    vec3 helper = abs(axis.z) < 0.9 ? vec3(0,0,1) : vec3(1,0,0);
    vec3 u = normalize(cross(axis, helper));
    vec3 v = cross(axis, u);
    vec2 fullResolution = sensorNormalPair(sensorKey(pixel, p, domain));
    float luma = fullResolution.x;
    vec3 greenRow = (transpose(p.workingToCamera * c))[1];
    float greenLuma = dot(greenRow,axis);
    float greenChromaU = dot(greenRow,u);
    float greenChromaV = dot(greenRow,v);
    // Difference and average have the SAME marginal variance. Thus G1/G2
    // have variance 2*V_G and zero covariance for every chroma amount.
    companion = fullResolution.y * sqrt(greenLuma*greenLuma +
        p.controls.y*p.controls.y*(greenChromaU*greenChromaU+greenChromaV*greenChromaV));
    vec2 chroma = sensorChroma(pixel, p, domain ^ 0xc2b2ae35u) * p.controls.y;
    return c * (axis*luma + u*chroma.x + v*chroma.y);
}

vec3 sensorGaussian(mat3 c, ivec2 pixel, SensorParameters p, uint domain)
{
    float companion;
    return sensorGaussian(c,pixel,p,domain,companion);
}

vec3 sensorReadGaussian(ivec2 pixel, SensorParameters p, out float companion)
{
    vec2 fullResolution = sensorNormalPair(sensorKey(pixel,p,0x7eadu));
    companion = fullResolution.y * p.controls.w;
    vec2 chroma = sensorChroma(pixel,p,0x7eadu ^ 0xc2b2ae35u) * p.controls.y;
    return p.readCameraBasis * vec3(fullResolution.x,chroma);
}

float sensorSafeLight(float value)
{
    // Negative radiance and NaN mean no collected light. Positive infinity
    // and enormous HDR values saturate safely, without overflow in M or K.
    if (isnan(value)) return 0.0;
    return clamp(value, 0.0, 1e6);
}

vec3 sensorSafeLight(vec3 value)
{
    return vec3(sensorSafeLight(value.x), sensorSafeLight(value.y), sensorSafeLight(value.z));
}

struct SensorReadout
{
    vec3 cameraElectrons; // signed, AFTER ADC black subtraction
    vec3 preClipElectrons; // captured BEFORE full-well / per-channel ADC clipping
    vec2 greenPreClipElectrons; // individual sites, before full well and ADC
    vec2 greenCameraElectrons; // individual sites, after amount-controlled ADC
};

// Fade quantisation error with noise to avoid under-dithered DN steps below 1.
vec2 sensorQuantise(vec2 dn, float amount)
{
    if (amount >= 1.0) return round(dn);
    if (amount <= 0.0) return dn;
    return dn + amount * (round(dn) - dn);
}

SensorReadout sensorReadCamera(vec3 cameraLinear, ivec2 pixel, SensorParameters p)
{
    vec3 mu = min(sensorSafeLight(cameraLinear) * (p.scale.y / p.scale.z), vec3(1e12));
    vec3 shotVariance = vec3(mu.x >= 20.0 ? mu.x : 0.0,
                            mu.y >= 20.0 ? mu.y*0.5 : 0.0,
                            mu.z >= 20.0 ? mu.z : 0.0);
    vec3 shotResidual = vec3(0.0);
    float greenShotDifference = 0.0;
    if (p.controls.x > 0.0)
    {
        if (any(greaterThan(shotVariance, vec3(0.0))))
        {
            mat3 c = sensorCholesky(sensorCovariance(p.cameraToWorking, shotVariance));
            shotResidual = p.workingToCamera * sensorGaussian(c, pixel, p, 0x5a07u,greenShotDifference);
        }
        // The two green photosites are averaged, giving variance mu_G/2.
        // Discrete Poisson residuals are NEVER spatially filtered.
        if (any(lessThan(mu,vec3(20.0))))
        {
            // Four inverse CDFs share one bounded recurrence. Each lane retains its
            // own independent uniform and lambda; no distribution is replaced.
            vec3 lowLambda = mix(vec3(0.0),mu,lessThan(mu,vec3(20.0)));
            vec4 counts = sensorPoisson4(lowLambda.xyyz,uvec4(sensorKey(pixel,p,0x101u),
                sensorKey(pixel,p,0x102u),sensorKey(pixel,p,0x103u),sensorKey(pixel,p,0x104u)));
            if (mu.x < 20.0) shotResidual.x = counts.x - mu.x;
            if (mu.y < 20.0)
            {
                shotResidual.y = 0.5*(counts.y+counts.z) - mu.y;
                greenShotDifference = 0.5*(counts.y-counts.z);
            }
            if (mu.z < 20.0) shotResidual.z = counts.w - mu.z;
        }
    }
    vec3 photons = mu + p.controls.x * shotResidual;
    vec3 readResidual = vec3(0.0);
    float greenReadCompanion = 0.0;
    if (p.controls.x > 0.0)
        readResidual = p.controls.x * sensorReadGaussian(pixel,p,greenReadCompanion);
    SensorReadout result;
    result.preClipElectrons = photons + readResidual;
    vec2 amplified = p.physics.z * (min(photons.xz, vec2(p.physics.x)) + readResidual.xz);
    vec2 dn = sensorQuantise(clamp(amplified + p.physics.w, 0.0, p.scale.x), p.controls.x);
    result.cameraElectrons.xz = (dn - p.physics.w) / p.physics.z;
    // Demosaiced green averages TWO photosites after each site's ADC.
    // At amount 1, averaging before quantisation would retain 1/12 DN^2 in G
    // instead of the required 1/24 (and would also clip the wells too late).
    float greenReadDifference = greenReadCompanion;
    vec2 greenPhotons = photons.yy + p.controls.x*greenShotDifference*vec2(1,-1);
    vec2 greenRead = readResidual.yy + p.controls.x*greenReadDifference*vec2(1,-1);
    result.greenPreClipElectrons = greenPhotons + greenRead;
    result.preClipElectrons.y = max(result.greenPreClipElectrons.x,result.greenPreClipElectrons.y);
    vec2 greenDN = sensorQuantise(clamp(p.physics.z*(min(greenPhotons,vec2(p.physics.x))+greenRead) +
                              p.physics.w, 0.0, p.scale.x), p.controls.x);
    result.cameraElectrons.y = (0.5*(greenDN.x+greenDN.y)-p.physics.w)/p.physics.z;
    result.greenCameraElectrons = (greenDN-p.physics.w)/p.physics.z;
    return result;
}

vec3 sensorHueClip(vec3 clipped, vec3 preClipElectrons, SensorParameters p)
{
    float eSat = min(p.physics.x, (p.scale.x-p.physics.w)/p.physics.z);
    vec3 coverage = smoothstep(vec3(p.scale.w * eSat),vec3(eSat),preClipElectrons);
    // Partial clipping rolls off progressively. Full neutral requires every
    // camera channel to saturate; the green lane is the per-site maximum.
    float t = (coverage.x+coverage.y+coverage.z)/3.0;
    return mix(clipped, vec3(dot(clipped, vec3(0.2126,0.7152,0.0722))), t);
}

vec3 applySensor(vec3 workingLinear, ivec2 pixel, SensorParameters p)
{
    vec3 cameraLinear = sensorSafeLight(p.workingToCamera * sensorSafeLight(workingLinear));
    SensorReadout result = sensorReadCamera(cameraLinear, pixel, p);
    vec3 linear = p.cameraToWorking * result.cameraElectrons * (p.controls.z / p.scale.y);
    return sensorHueClip(linear, result.preClipElectrons, p);
}
