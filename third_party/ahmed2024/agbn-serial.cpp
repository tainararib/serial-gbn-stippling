/*
 * Serial GBN stippling.
 * This is a working demo code provided as a supplementary code with the paper:
 *      Serial Gaussian-Blue-Noise Stippling
 * The code is provided as is, without warrantees.
 * Apologies for the many testing options, as this is a research version.
 * Please contact me at abdalla_gafar@hotmail.com if you have questions
 *  or discussions.
 *
 * 2023-08-27: First serial (CPU) implementation by Abdalla Ahmed
 * 2023-06-18: Last revision for submission by Abdalla Ahmed
 */

#include <vector>
#include <stdlib.h>
#include <stdio.h>
#include <getopt.h>
#include <cmath>
#include <string>
#include <signal.h>
#include <stdint.h>
#include <numeric>                                                              // For std::accumulate
#include <algorithm>                                                            // For std::max_element
#include <fstream>

#define USE_DOUBLE

#include "common.h"
#include "2048.h"

#ifdef USE_DOUBLE
    #define Float double
    #define SUPPORT 9
#else
    #define Float float
    #define SUPPORT 6
#endif


// =============================================================================
// Global Parameters
// =============================================================================

static bool interruptFlag = false;                                              // To stop the optimization manually
Float terminateThreshold = 0;
Float stepCap = 0.5;                                                            // Relative to nearest neighbor distance
bool areaPixels = false;
bool parallelMode = false;
int scalingMode = 1;
bool restrictToNonZero = false;
bool randomOrder = false;
bool transparent = true;
Float power = 1;

// =============================================================================
// Utils
// =============================================================================

// -----------------------------------------------------------------------------
// Populate a randomly ordered list
// -----------------------------------------------------------------------------

inline void shuffle(std::vector<int> &list) {                                   // Populate a randomly ordered list.
    int N = list.size();
    for (int i = 0; i < N; i++) list[i] = i;
    for (int i = N - 1; i > 0; i--) {                                           // Iterate down the list, and swap each place with another place down, inclusive of same place.
        int r = rnd() % (i + 1);                                                // (i + 1) means that the same place is included.
        std::swap(list[i], list[r]);
    }
}

// -----------------------------------------------------------------------------
// Return square distance between two points
// -----------------------------------------------------------------------------

inline Float distSq(const Point &p1, const Point &p2) {
    Float dx = p2.x - p1.x;
    Float dy = p2.y - p1.y;
    return dx * dx + dy * dy;
}

// -----------------------------------------------------------------------------
// Return max travelled distance between two point sets
// -----------------------------------------------------------------------------

Float maxDistance(const Points &p1, const Points &p2) {
    Float rrMax(0);
    for (int i = 0; i < p1.size(); i++) {
        rrMax = std::max(rrMax, distSq(p1[i], p2[i]));
    }
    return std::sqrt(rrMax);
}


// =============================================================================
// Adaptive Class
// =============================================================================

class GBNAdaptive {
private:
    int N;                                                                      // Number of points
    std::vector<Point> p, pRef;                                                 // List of points
    Float timeStep;                                                             // Scaling factor of gradient steps
    int w, h;                                                                   // Width and height of domain
    int n;                                                                      // Size, max(w, h), needed to maintain aspect ratio
    int initMode;                                                               // Initization mode
    Float sigma;                                                                // Sigma of energy kernel
    Float sigmaPixel;                                                           // Filtering sigma of pixels; mainly for testing; e.g. 0 means Dirac deltas
    std::vector<Float> bmp;                                                     // Pixels
    std::vector<Float> v;                                                       // kernel shaping factors (area)
    std::vector<int> nearestNeighbor;                                           // Index of nearest neighbor
    std::vector<int> neighbors;                                                 // List of considered neighbors for each site
    std::vector<int> indx0;                                                     // List of first index of each site in neighbors list
    Float unitArea, unitAreaInv;
    void init();                                                                // Initialize parameters and buffers
    inline void updateKernel(int i);
    void initKernels();                                                         // Initialize kernel shaping factors
    int updateNeighborsList();
    // -------------------------------------------------------------------------
    // Recording parameters
    // -------------------------------------------------------------------------
    Float outBlackRatio;                                                        // Ratio of black in produced renderings; 1 is full black
    std::string frameBaseName;                                                  // For saving frames during optimization
    int frameFormat;                                                            // 1: txt, 2: eps, 4: png, 8: pdf
    Float outputResolution;                                                     // For deciding if there is a noticeable difference
    int frameIndex;                                                             // Index of output frame
    int frameCount;                                                             // If set, frames would cycle through this loop size, rewriting older ones
    int accumIterations;                                                        // Statistics
    int W, H;                                                                   // Width and height of recorded frames
    Float margin;                                                               // Margin in plots, given as a ratio to domain size
    Float unit;                                                                 // Unit of recorded frames
    std::string getParameters();                                                // Write current parameters to a single line string
public:
    GBNAdaptive(                                                                // Construct from scratch
        int N,                                                                  // Number of points
        Image img,                                                              // Density map
        int initMode = 0                                                        // 0: full-random, 1: weighted random, 2: 2048
    );
    GBNAdaptive(char *txtFileName, const Image img);                            // Load a given point set
    void setSigma(Float value);                                                 // Set sigma to a given value
    void setSigmaPixel(Float value) { sigmaPixel = value; };
    void setTimeStep(Float value) {timeStep = value;};
    void setOutBlackRatio(Float value) { outBlackRatio = value; };
    void setFrameBaseName(char *value) { frameBaseName = value; };
    void setFrameFormat(int value) { frameFormat = value; };
    void setFrameCount(int value) { frameCount = value; };
    void setMargin(Float value) { margin = value; };
    void setFrameSize(int W, int H);
    void saveEPS(std::string fileName);
    void saveTXT(std::string fileName);
    void savePNG(std::string fileName, bool drawTransparent = transparent);
    void savePDF(std::string fileName);
    void save(std::string fileName);
    void snapshot(int iteration);
    void optimize(int iterations);
    ~GBNAdaptive();
};

// -----------------------------------------------------------------------------
// Construct from scratch
// -----------------------------------------------------------------------------

GBNAdaptive::GBNAdaptive(
    int N, Image img, int initMode
) : N(N), bmp(img.pixels), w(img.w), h(img.h), initMode(initMode) {
    init();                                                                     // Common initialization tasks
    switch (initMode) {
        case 0: {
            for (int i = 0; i < N; i++) { p[i] = {rand(w), rand(h)}; }
            break;
        }
        case 1: default: {
            Float maxPixel = *std::max_element(bmp.begin(), bmp.end());         // Highest pixel density; high is darker
            for (int i = 0; i < N; /*conditionally increment inside loop*/) {   // Insert points proportional to density using rejection sampling
                Float x = rand(w);                                              // Random x in [0, w)
                Float y = rand(h);                                              // Random y in [0, h)
                Float threshold = rand(maxPixel);                               // Random threshold
                if (bmp[int(y) * w + int(x)] > threshold) {                     // Is underlying pixel above threshold?
                    p[i++] = {x, y};                                            // If yes, insert a point at this location
                }
            }
            break;
        }
        case 2: {
            C2048 slicer(img);
            p = slicer.slice(N);
            break;
        }
    }
}

// -----------------------------------------------------------------------------
// Load a given point set
// -----------------------------------------------------------------------------

GBNAdaptive::GBNAdaptive(char *txtFileName, Image img)
: bmp(img.pixels), w(img.w), h(img.h) {
    std::fstream file(txtFileName);
    file >> N;
    init();                                                                     // Now that N is known
    Float x, y;
    for (int i = 0; i < N; i++) {
        file >> x >> y;
        if (file.eof()) {
            fprintf(stderr, "Failed to load all points\n");
            exit(1);
        }
        p[i] = {x * n, y * n};
    }
    file.close();
}

// -----------------------------------------------------------------------------
// Initialize parameters and buffers
// -----------------------------------------------------------------------------

void GBNAdaptive::init() {
    unitArea = Float(h * w) / N;
    unitAreaInv = 1 / unitArea;
    p.resize(N);
    v.resize(N);
    nearestNeighbor.resize(N);
    neighbors.resize(500 * N);                                                  // Assuming 20x20 grid around each site plus a safety margin
    indx0.resize(N + 1);                                                        // We need to find the end for the last site.
    // -------------------------------------------------------------------------
    // Recording parameters
    // -------------------------------------------------------------------------
    n = std::max(w, h);                                                         // Square domain size to preserve aspect ratio
    unit = 1. / n;
    setFrameSize(w, h);                                                         // Default saved frame size to input image size
    setFrameFormat(4);                                                          // Default frame saving format to png
    accumIterations = 0;                                                        // Reset recording parameters
    frameIndex = 0;
    frameCount = 0;                                                             // Use iteration number for indexing
    margin = 0;
    // -------------------------------------------------------------------------
    Float mass = std::accumulate(bmp.begin(), bmp.end(), Float(0));             // Sum of all pixels
    outBlackRatio = mass / (w * h);                                             // Default output rendering black level to match input image
    for (int i = 0; i < w * h; i++) { bmp[i] *= (N / mass); }                   // Make mass of pixels = mass of points
    Float maxDensity = *std::max_element(bmp.begin(), bmp.end()) * unitArea;
    fprintf(
        stderr, "Highest density relative to average = %f\n",
        maxDensity
    );
    setSigmaPixel(0.5);                                                         // 0: deltas; < 0 means use same sigma
    setSigma(1);
    //setTimeStep(0.5);                                                           // Our default, after long experimentation
    setTimeStep(0.5 / maxDensity);
}

// -----------------------------------------------------------------------------
// Destructor
// -----------------------------------------------------------------------------

GBNAdaptive::~GBNAdaptive() {
}

// -----------------------------------------------------------------------------
// Set energy sigma
// -----------------------------------------------------------------------------

void GBNAdaptive::setSigma(Float value) {
    if (value == 93) value = 0.930604859102099599;                              // sqrt(sqrt(3)/2), indicating triangular grid
    sigma = value                                                               // Input energy kernel sigma
            / sqrt(2)                                                           // This is the filtering kernel
            / sqrt(2)                                                           // Account for the pixels, 50% share
            * sqrt(unitArea)                                                    // Scale to point area
    ;
    fprintf(stderr, "Energy sigma set to = %f, %f pixels\n", value, sigma);
}

// -----------------------------------------------------------------------------
// Compute list of neighbors
// -----------------------------------------------------------------------------

int GBNAdaptive::updateNeighborsList() {
    const Float supportSqRef = SUPPORT * SUPPORT * sigma * sigma;
    Float ssRef = sigma * sigma;
    int k = 0;
    int neighborCountMax(0);
    for (int i = 0; i < N; i++) {
        indx0[i] = k;
        for (int j = 0; j < N; j++) {
            if (j == i) continue;
            Float supportSq = supportSqRef * (v[i] + v[j]);
            Float rr = distSq(p[i], p[j]);
            if (rr < supportSq) {
                neighbors[k++] = j;                                             // Register j as a neighbor of i.
            }
        }
        neighborCountMax = std::max(neighborCountMax, k - indx0[i]);
    }
    indx0[N] = k;                                                               // Cap
    return neighborCountMax;
}


// -------------------------------------------------------------------------
// Update kernel shaping factor of a specific point
// -------------------------------------------------------------------------

void GBNAdaptive::updateKernel(int i) {
    Float ddMin(1e12);
    int minIndex(-1);
    for (int k = indx0[i]; k < indx0[i+1]; k++) {
        int j = neighbors[k];
        Float dd = distSq(p[i], p[j]);
        if (ddMin > dd) {
            ddMin = dd;
            minIndex = j;
        }

    }
    v[i] = ddMin * unitAreaInv;
    nearestNeighbor[i] = minIndex;
}

// -------------------------------------------------------------------------
// Initialize kernel shaping factors
// -------------------------------------------------------------------------
void GBNAdaptive::initKernels() {
    for (int i = 0; i < N; i++) {
        Float ddMin(1e12);
        int minIndex(-1);
        for (int j = 0; j < N; j++) {
            if (j == i) continue;
            Float dd = distSq(p[i], p[j]);
            if (ddMin > dd) {
                ddMin = dd;
                minIndex = j;
            }
        }
        v[i] = ddMin * unitAreaInv;
        nearestNeighbor[i] = minIndex;
    }
}

// -----------------------------------------------------------------------------
// Optimization Routine
// -----------------------------------------------------------------------------


void GBNAdaptive::optimize(int iterations) {
    // -------------------------------------------------------------------------
    // Set optimization parameters
    // -------------------------------------------------------------------------
    bool isRecorded =  frameFormat;                                             // If recording is requested
    Float ss2InvNeg = -1 / (2 * sigma * sigma);
    initKernels();
    if (sigmaPixel < 0) {                                                       // Instruction to use the same sigma as the points, noting that ddMin of pixels is 1
        sigmaPixel = sigma * sqrt(unitAreaInv);
    }
    Float vPixel = (sigmaPixel/sigma) * (sigmaPixel/sigma);
    int neighborCountMax = updateNeighborsList();
    const Float NInv = Float(1) / N;
    Points pNeighborsRef = p;                                                   // Needed to update neighbors list
    pRef = p;                                                                   // Copy point set as a reference for recording
    Float stepCapSq = stepCap * stepCap * unitArea;
    if (isRecorded) snapshot(0);
    fprintf(stderr, "%s\n", getParameters().c_str());
    // -------------------------------------------------------------------------
    // Optimization loop
    // -------------------------------------------------------------------------
    std::vector<Float> gx(w), ex(w), gy(h), ey(h);
    Float gradLength, gradLengthMax, gradLengthAvg(n), gradLengthSum;
    Points pNext;                                                               // For parallel mode
    if (parallelMode) pNext.resize(N);
    Float timeStep = this->timeStep;
    //if (2 * sigma > sqrt(unitArea)) {                                           // If supplied sigma > 1
        timeStep *= unitArea / (4 * sigma * sigma);
    //}
    for (int itrt = 0; itrt < iterations && !interruptFlag; itrt++) {
        Float stepSqMax(0);                                                     // Square of largest displacement in this iteration
        int reIterated(0);
        int clampedCount(0);
        gradLengthSum = 0;
        gradLengthMax = 0;
        for (int i = 0; i < N; i++) {                                           // Iterate through all points
            if (distSq(p[i], pNeighborsRef[i]) > 0.25 * unitArea * v[i]) {      // If a point moves more than half distance to nearest neighbor
                neighborCountMax = updateNeighborsList();                       // Update all neighbors map
                pNeighborsRef = p;                                              // Update reference locations
                break;                                                          // No need to continue with the loop, then
            }
        }
        std::vector<int> order(N);
        if (randomOrder) shuffle(order);
        for (int k = 0; k < N && !interruptFlag; k++) {                         // Iterate through all points
            int i = randomOrder ? order[k] : k;
            if (parallelMode) pNext[i] = p[i];                                  // Fall back in case of exceptions
            Vector grad = {0, 0};                                               // Gradient
            Float t = timeStep;
            if (scalingMode) t *= pow(v[i], 0.5 * scalingMode);
            // -----------------------------------------------------------------
            // Attraction of image
            // -----------------------------------------------------------------
            Float support = SUPPORT * sigma * std::sqrt(v[i] + vPixel);
            int Xmin = std::max(floor(p[i].x - support), Float(    0));
            int Xmax = std::min( ceil(p[i].x + support), Float(w - 1));
            int Ymin = std::max(floor(p[i].y - support), Float(    0));
            int Ymax = std::min( ceil(p[i].y + support), Float(h - 1));
            Float a = 1 / (v[i] + vPixel);                                      // Kernel shaping factor
            ///*test*/ a = 1 / (v[i] + v[i] * unitAreaInv);
            //Float a = 1 / ((1 + vPixel)*v[i]);                                  // Kernel shaping factor
            if (areaPixels) {                                                   // Integrate gradient over pixel area
                Float c = -a * ss2InvNeg;
                Float sqrtC = sqrt(c);
                for (int Y = Ymin, row = 0; Y <= Ymax; Y++, row++) {
                    Float yb(p[i].y - Y - 1), yt(yb + 1);
                    gy[row] = exp(-c * yt * yt) - exp(-c * yb * yb);
                    ey[row] = erf(sqrtC * yb) - erf(sqrtC * yt);
                }
                for (int X = Xmin, col = 0; X <= Xmax; X++, col++) {
                    Float xl(p[i].x - X - 1), xr(xl + 1);
                    gx[col] = exp(-c * xr * xr) - exp(-c * xl * xl);
                    ex[col] = erf(sqrtC * xl) - erf(sqrtC * xr);
                }
                for (int Y = Ymin, row = 0; Y <= Ymax; Y++, row++) {
                    for (int X = Xmin, col = 0; X <= Xmax; X++, col++) {
                        grad.x -= bmp[Y * w + X] * gx[col] * ey[row];
                        grad.y -= bmp[Y * w + X] * gy[row] * ex[col];
                    }
                }
                grad *= sqrt(M_PI) / (4 * pow(c, 1.5));
            }
            else {                                                              // Sample gradient at pixel points
                for (int Y = Ymin, row = 0; Y <= Ymax; Y++, row++) {
                    Float dy = p[i].y - (Y + Float(0.5));
                    gy[row] = std::exp(a * ss2InvNeg * dy * dy);
                }
                for (int X = Xmin, col = 0; X <= Xmax; X++, col++) {
                    Float dx = p[i].x - (X + Float(0.5));
                    gx[col] = std::exp(a * ss2InvNeg * dx * dx);
                }
                for (int Y = Ymin, row = 0; Y <= Ymax; Y++, row++) {
                    Float dy = p[i].y - (Y + Float(0.5));
                    for (int X = Xmin, col = 0; X <= Xmax; X++, col++) {
                        Float dx = p[i].x - (X + Float(0.5));
                        Float g = gx[col] * gy[row];
                        g *= bmp[Y * w + X];
                        grad.x -= dx * g;
                        grad.y -= dy * g;
                    }
                }
            }
            grad *= a * a;
            // -----------------------------------------------------------------
            // Repulsion of other points:
            // -----------------------------------------------------------------
            for (int k = indx0[i]; k < indx0[i+1]; k++) {
                int j = neighbors[k];
                Float dx = p[i].x - p[j].x;
                Float dy = p[i].y - p[j].y;
                Float rr = dx * dx + dy * dy;
                Float a = 1 / (v[i] + v[j]);
                Float g = a * a * std::exp(a * ss2InvNeg * rr);
                grad.x += dx * g;
                grad.y += dy * g;
            }
            // -----------------------------------------------------------------
            // Update point:
            // -----------------------------------------------------------------
            gradLength = std::sqrt(grad.sq());
            gradLengthSum += gradLength;
            gradLengthMax = std::max(gradLengthMax, gradLength);
            grad *= t;
            Float displacementMaxSq = stepCapSq * v[i];                         // Cap relative to nearest neighbor distance
            if (grad.sq() > displacementMaxSq) {
                grad *= std::sqrt(displacementMaxSq / grad.sq());               // Clamp to max allowed displacement
                clampedCount++;                                                 // Collect information if needed
            }
            Float x = p[i].x + grad.x;
            Float y = p[i].y + grad.y;
            if (x < 0 || x >= w || y < 0 || y >= h) continue;                   // Restrict to image boundary. In theory it should not be needed
            if (restrictToNonZero && !bmp[int(y) * w + int(x)]) continue;
            stepSqMax = std::max(stepSqMax, grad.sq());
            if (parallelMode) {
                pNext[i] = {x, y};                                              // Set target location
                continue;                                                       // Shaping factors will be updated later
            }
            // -----------------------------------------------------------------
            // Update point location
            // -----------------------------------------------------------------
            p[i] = {x, y};
            // -----------------------------------------------------------------
            // Update shaping factor of self and affected neighbors
            // -----------------------------------------------------------------
            Float ddMin(1e12);
            int minIndex(-1);
            for (int k = indx0[i]; k < indx0[i+1]; k++) {
                int j = neighbors[k];
                Float dd = distSq(p[i], p[j]);
                if (ddMin > dd) {
                    ddMin = dd;
                    minIndex = j;
                }
                if (dd * unitAreaInv < v[j]) {
                    v[j] = dd * unitAreaInv;
                    nearestNeighbor[j] = i;
                }
                if (nearestNeighbor[j] == i && dd * unitAreaInv > v[j]) {       // Was nearest neighbor and moved away
                    updateKernel(j);                                            // Search for a new nearest neighbor
                }
            }
            v[i] = ddMin * unitAreaInv;
            nearestNeighbor[i] = minIndex;
        }
        if (parallelMode) {
            std::swap(p, pNext);                                                // Copy all new point locations
            for (int i = 0; i < N; i++) {
                updateKernel(i);
            }
        }
        gradLengthAvg = gradLengthSum * NInv;
        fprintf(
            stderr,
            "\rItrtn %d: "
            "stepMax = %e, "
            "#clamped = %4d, "
            "gradLenAvg/Max = %e/%e",
            itrt,
            sqrt(stepSqMax) / outputResolution,
            clampedCount,
            gradLengthAvg, gradLengthMax
        );
        if (isRecorded) {
            accumIterations++;
            Float dmax = maxDistance(p, pRef);
            fprintf(
                stderr,
                ", Frame %5d: accum max displacement = %19.17f in %4d iterations",
                frameIndex, dmax / outputResolution, accumIterations
            );
            if (dmax > outputResolution) {
                snapshot(itrt + 1);
                pRef = p;
                accumIterations = 0;                                                // Reset iterations counter
                dmax = 0;
            }

        }
        //if (gradLengthMax < terminateThreshold) break;
        if (sqrt(stepSqMax) / outputResolution < terminateThreshold) break;
    }
    fprintf(stderr, "\n");
}



// -----------------------------------------------------------------------------
// Record frame if condition is met
// -----------------------------------------------------------------------------

void GBNAdaptive::setFrameSize(int W, int H) {
    this->W = W;
    this->H = H;
    outputResolution = (Float(n) / std::max(W, H));                             // Pixel in output resolution
}

void GBNAdaptive::snapshot(int iteration) {
    char serial[10];
    int seqNo = frameCount ? frameIndex % frameCount : iteration;
    sprintf(serial, "%08d", seqNo);
    if (frameFormat & 1) {
        saveTXT(frameBaseName + serial + ".txt");
    }
    if (frameFormat & 2) {
        saveEPS(frameBaseName + serial + ".eps");
    }
    if (frameFormat & 4) {
        savePNG(frameBaseName + serial + ".png");
    }
    if (frameFormat & 8) {
        savePDF(frameBaseName + serial + ".pdf");
    }
    if (frameFormat & 16) {
        savePNG("-", false);                                                    // Turn transparency off for video recording
    }
    frameIndex++;
}

// -----------------------------------------------------------------------------
// Obtain a line of current parameters
// -----------------------------------------------------------------------------

std::string GBNAdaptive::getParameters() {
    char buffer[1024];
    sprintf(
        buffer,
        "Code file agbn-serial.cpp, "
        "g = %f, "
        "G = %f, "
        "t = %f, "
        "stepCap = %f, "
        "I = %d, "
        "q = %3.1f, "
        "scaling with v[i]^%0.1f, "
        "area pixels = %s, "
        "parallel mode = %s, "
        "scanning order = %s, ",
        sigma * 2 * sqrt(unitAreaInv),
        sigmaPixel,
        timeStep,                                                               // * unitArea / (4 * sigma * sigma),
        stepCap,
        initMode,
        power,
        0.5 * scalingMode,
        areaPixels ? "true" : "false",
        parallelMode ? "true" : "false",
        randomOrder ? "random" : "fixed"
    );
    return buffer;
}

// -----------------------------------------------------------------------------
// Saving Point Sets
// -----------------------------------------------------------------------------

void GBNAdaptive::save(std::string fileName) {
    std::string extension = fileName.substr(fileName.length() - 4, 4);
    transform(
        extension.begin(), extension.end(), extension.begin(), ::tolower
    );
    if      (extension == ".txt") { saveTXT(fileName.c_str()); }
    else if (extension == ".eps") { saveEPS(fileName.c_str()); }
    else if (extension == ".pdf") { savePDF(fileName.c_str()); }
    else if (extension == ".png") { savePNG(fileName.c_str()); }
    else {
        fprintf(stderr, "Unknown file extension %s\n", extension.c_str());
    }
}

void GBNAdaptive::saveTXT(std::string fileName) {
    FILE *file = fopen(fileName.c_str(), "w");
    fprintf(file, "%d\n", N);
    for (int i = 0; i < N; i++) {
        fprintf(file, "%0.17f %0.17f\n", unit * p[i].x, unit * p[i].y);
    }
    fclose(file);
}

void GBNAdaptive::saveEPS (std::string fileName) {
    FILE *file = fopen(fileName.c_str(), "w");
    fprintf(
        file,
        "%%!PS-Adobe-3.0 EPSF-3.0\n"
        "%%%%BoundingBox: 0 0 %d %d\n"
        "%% %s\n"
        "/N %d def\n"
        "/w %d def\n"
        "/h %d def\n"
        "/n %d def\n"
        "/blackRatio %f def\n"
        "/margin %f def\n"
        "/PI 3.141592654 def\n"
        "/r w h mul n n mul div N div blackRatio mul PI div sqrt def\n"
        "/p {r 0 360 arc fill} def\n"
        "0 setlinewidth\n"
        "%d 10 mul dup scale\n"
        "1 1 margin 2 mul add div dup scale margin dup translate\n",
        10 * W, 10 * H,
        getParameters().c_str(),
        N, w, h, n, outBlackRatio, margin, std::max(W, H)
    );
    for (int i = 0; i < N; i++) {
        fprintf(file, "%0.17f %0.17f p\n", unit * p[i].x, unit * p[i].y);
    }
    fprintf(file, "showpage\n");
    fclose(file);
}

void GBNAdaptive::savePNG (std::string fileName, bool transparent) {
    int scale = std::max(H, W);
    int pixelMargin = margin * scale;
    scale += 2 * pixelMargin;                                                   // Margin is added as a frame to given pixel dimensions
    cairo_format_t format = (
        transparent ? CAIRO_FORMAT_ARGB32 : CAIRO_FORMAT_RGB24
    );
    cairo_surface_t *surface = cairo_image_surface_create(
        format, W + 2 * pixelMargin, H + 2 * pixelMargin
    );
    cairo_t *cr = cairo_create(surface);
    Float area = std::min(w, h) / Float(n);
    const double r = sqrt(area * outBlackRatio / N / M_PI);
    cairo_identity_matrix(cr);
    cairo_translate(cr, 0, H + 2 * pixelMargin);
    cairo_scale(cr, scale, -scale);
    cairo_scale(cr, 1./(1 + 2 * margin), 1./(1 + 2 * margin));
    cairo_translate(cr, margin, margin);
    if (transparent) { cairo_set_source_rgba(cr, 1.0, 1.0, 1.0, 0); }
    else             { cairo_set_source_rgb (cr, 1.0, 1.0, 1.0   ); }
    cairo_paint(cr);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    for (int i = 0; i < N; ++i) {
        cairo_arc(cr, unit * p[i].x, unit * p[i].y, r, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_destroy (cr);
    if (fileName == "-") {
        cairo_surface_write_to_png_stream(surface, &png2stdout, NULL);
    }
    else {
        cairo_surface_write_to_png(surface, fileName.c_str());
    }
    cairo_surface_destroy(surface);
}

void GBNAdaptive::savePDF (std::string fileName) {
    cairo_surface_t *surface = cairo_pdf_surface_create(
        fileName.c_str(), 4000, 4000
    );
    cairo_t *cr = cairo_create(surface);
    int scale = std::max(H, W);
    Float area = std::min(w, h) / Float(n);
    const double r = sqrt(area * outBlackRatio / N / M_PI);
    cairo_identity_matrix(cr);
    cairo_translate(cr, 0, 4000);
    cairo_scale(cr, 4000, -4000);
    cairo_scale(cr, 1./(1 + 2 * margin), 1./(1 + 2 * margin));
    cairo_translate(cr, margin, margin);
    cairo_set_source_rgba(cr, 0, 0, 0, 1);
    for (int i = 0; i < N; ++i) {
        cairo_arc(cr, unit * p[i].x, unit * p[i].y, r, 0, 2 * M_PI);
        cairo_fill(cr);
    }
    cairo_show_page(cr);
    cairo_destroy (cr);
    cairo_surface_destroy (surface);
}



// =============================================================================
// User Interruption Handling
// =============================================================================

static void signalHandler(int signalCode) {
    fprintf(stderr, "Aborting ...\n");
    interruptFlag = true;
}

const char *USAGE_MESSAGE =
"Usage:"
" %s [options] <image.pgm> <point count> <iterations> "
"[output.abc output.efg]\n"
"Supported save types are .txt, .eps, .png, .pdf\n"
"Options:\n"
" -g <sigma>            User-supplied sigma; default 1\n"
" -G <sigma>            Filtering sigma of pixels; default 0.5\n"
" -A                    Use area pixels; default is points\n"
" -S <scalingMode>      Scale timeStep by a power (0-4) of sqrt(v[i]); "
                       "default 1\n"
" -t <time step>        Scale factor for gradient; default 0.5\n"
" -T <threshold>        Terminate optimization when longest grad goes below"
                       "threshold\n"
" -c <outBlackRatio>    Coverage ratio of black in renderings, "
                       "default is computed from image mean\n"
" -l <fileName>         Load points from txt file\n"
" -f <frameName>        Take snapshots named frameName%%07d.ext\n"
" -F <format>           Bitmask of frame name extensions: "
                       "1: txt, 2: eps, 4: png, 8: pdf; default is 4.\n"
" -W/-w <width>         Width of recorded frames; default is image width\n"
" -H/-h <height>        Height of recorded frames; default is image height\n"
" -Q <#frames>          Recycle through this number of frame indices; "
                       "default is to index by iteration numbers.\n"
" -M <margin>           Add a fractional margin in plots; default is not\n"
" -q <power>            Raise pixels to power; "
                       "use 2 pixels for line-based rendering\n"
" -I <initialization>   0: random, 1: weighted random (default), 2: 2048\n"
" -X <stepCap>          Relative to nearest neighbor distance; default 0.5\n"
" -P                    parallelMode: defer update of points like in GPU; "
                       "default is serial: update each point immediately\n"
" -O                    Randomize scanning order; default is fixed order\n"
" -Z                    Restrict movement to non-Zero pixels\n"
" -o                    Make png plots opaque; default is transparent\n"
;


int main(int argc,char **argv) {
    int opt;
    char *inputFileName = NULL;
    char *frameBaseName = NULL;
    Float t = 0;
    Float sigma = 0;
    Float sigmaPixel(0);
    Float outBlackRatio = 0;
    int W = 0, H = 0;
    int frameFormat = -1;
    bool terminateOnNoMove = false;
    int initMode = 1;
    Float margin = 0;
    int frameCount = 0;
    while (
        (opt = getopt(
            argc, argv, "g:G:AS:t:c:l:f:W:w:H:h:F:T:M:q:Q:I:X:ZOo")
        ) != -1
    ) {
        switch (opt) {
            case 'g': sigma = atof(optarg); break;
            case 'G': sigmaPixel = atof(optarg); break;
            case 'A': areaPixels = true; break;
            case 'S': scalingMode = atoi(optarg); break;
            case 't': t = atof(optarg); break;
            case 'c': outBlackRatio = atof(optarg); break;
            case 'l': inputFileName = optarg; break;
            case 'f': frameBaseName = optarg; break;
            case 'F': frameFormat = atoi(optarg); break;
            case 'W': case 'w': W = atoi(optarg); if (!H) H = W; break;
            case 'H': case 'h': H = atoi(optarg); if (!W) W = H; break;
            case 'T': terminateThreshold = atof(optarg); break;
            case 'I': initMode = atoi(optarg); break;
            case 'M': margin = atof(optarg); break;
            case 'q': power = atof(optarg); break;
            case 'Q': frameCount = atoi(optarg); break;
            case 'X': stepCap = atof(optarg); break;
            case 'Z': restrictToNonZero = true; break;
            case 'O': randomOrder = true; break;
            case 'o': transparent = false; break;
            default: fprintf(stderr, USAGE_MESSAGE, argv[0]); exit(1);
        }
    }
    if (optind > argc - 3) {
        fprintf(stderr, USAGE_MESSAGE, argv[0]);
        exit(1);
    }
    signal(SIGINT, signalHandler);
    char *imageFileName = argv[optind++];
    int N = atoi(argv[optind++]);
    int iterations = atoi(argv[optind++]);
    Image img = loadPGM(imageFileName, power);
    GBNAdaptive gbn = (
        inputFileName ?
        GBNAdaptive(inputFileName, img) :
        GBNAdaptive(N, img, initMode)
    );
    if (sigma) gbn.setSigma(sigma);
    if (sigmaPixel) gbn.setSigmaPixel(sigmaPixel);
    if (t) gbn.setTimeStep(t);
    if (outBlackRatio) gbn.setOutBlackRatio(outBlackRatio);
    if (frameBaseName) gbn.setFrameBaseName(frameBaseName);
    if (frameFormat >= 0) gbn.setFrameFormat(frameFormat);
    if (frameCount) gbn.setFrameCount(frameCount);
    if (W) gbn.setFrameSize(W, H);
    if (margin) gbn.setMargin(margin);
    clock_t t0 = clock();
    gbn.optimize(iterations);
    clock_t t1 = clock();
    double totalTime = (double)(t1 - t0) / CLOCKS_PER_SEC;
    fprintf(stderr, "\ndone! Total time = %.6fs\n", totalTime);
    // =========================================================================
    // Save point sets
    // =========================================================================
    while (optind < argc) { gbn.save(argv[optind++]); }

}
