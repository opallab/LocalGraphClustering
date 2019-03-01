/**
 * Randomized proximal coordinate descent for l1 regularized pagerand vector
 * INPUT:
 *     alpha     - teleportation parameter between 0 and 1
 *     rho       - l1-reg. parameter
 *     v         - Seed node
 *     ai,aj,a   - Compressed sparse row representation of A
 *     d         - vector of node strengths
 *     epsilon   - accuracy for termination criterion
 *     n         - size of A
 *     ds        - the square root of d
 *     dsinv     - 1/ds
 *     offset    - offset for zero based arrays (matlab) or one based arrays (julia)
 *
 * OUTPUT:
 *     p              - PageRank vector as a row vector
 *     not_converged  - flag indicating that maxiter has been reached
 *     grad           - last gradient
 *
 */


// include write data to files
#include <iostream>
#include <fstream>
// end
#include <vector>
#include <cmath>
#include <ctime>
#include "include/routines.hpp"
#include "include/proxl1PRrand_c_interface.h"

using namespace std;

namespace proxl1PRrand 
{
bool fileExist(const string& fname) {
    ifstream file(fname);
    return file.good();
}

template<typename vtype, typename itype>
void writeQdiag(vtype numNodes, const string& fname, itype* ai, vtype* aj, double* d, double alpha) {
    if (fileExist(fname)) {
        return;
    }
    vector<bool> Adiag(numNodes, false);

    for (vtype node = 0; node < numNodes; ++node) {
        itype idx = ai[node];
        while (idx < ai[node + 1] && aj[idx] < node) ++idx;
        Adiag[node] = idx < ai[node + 1] && aj[idx] == node;
    }

    double c = (1 - alpha) / 2;
    ofstream file(fname);
    if (file.is_open()) {
        for (vtype node = 0; node < numNodes; ++node) {
            file << (1.0 - c - c * Adiag[node] / d[node]);
            if (node < numNodes - 1) file << ',';
        }
        file.close();
    }
}

template<typename vtype>
void writeLog(vtype numNodes, const string& fname, double* q) {
    ofstream file(fname, fstream::app);
    if (file.is_open()) {
        for (vtype i = 0; i < numNodes; ++i) {
            file << q[i];
            if (i < numNodes - 1) file << ',';
        }
        file << '\n';
        file.close();
    }
}

template<typename vtype, typename itype>
void writeGraph(vtype numNodes, const string& fname, itype* ai, vtype* aj) {
    if (fileExist(fname)) {
        return;
    }
    ofstream file(fname);
    if (file.is_open()) {
        itype idx = 0;
        for (vtype node = 0; node < numNodes; ++node) {
            for (vtype neighbor = 0; neighbor < numNodes; ++neighbor) {
                if (idx < ai[node + 1] && aj[idx] == neighbor) {
                    ++idx;
                    file << 1;
                } else {
                    file << 0;
                }
                if (neighbor < numNodes - 1) file << ',';
            }
            file << '\n';
        }
        file.close();
    }
}

void writeTime(clock_t& timeStamp, const string& fname) {
    clock_t currTime = clock();
    ofstream file(fname, fstream::app);
    if (file.is_open()){
        file << double(currTime - timeStamp) / CLOCKS_PER_SEC << '\n';
        file.close();
    }
    timeStamp = currTime;
}

static long long g_seed = 1;

inline
long long getRand() {
    g_seed = (214013*g_seed+2531011); 
    return (g_seed>>16)&0x7FFF; 
}

template<typename vtype, typename itype>
void updateGrad(vtype& node, double& stepSize, double& c, double& ra, double* q, double* grad, double* ds, double* dsinv, itype* ai, vtype* aj, bool* visited, vtype* candidates, vtype& candidatesSize) {
    double dqs = grad[node]-ds[node];
    double dq = dqs*stepSize;
    double cdq = c*dq;
    double cdqdsinv = cdq*dsinv[node];
    q[node] += dq;
    grad[node] -= dqs;

    vtype neighbor;
    for (itype j = ai[node]; j < ai[node + 1]; ++j) {
        neighbor = aj[j];
        grad[neighbor] += cdqdsinv*dsinv[neighbor]; //(1 + alpha)
        if (!visited[neighbor] && q[neighbor] + grad[neighbor] >= ds[neighbor]) {
            visited[neighbor] = true;
            candidates[candidatesSize++] = neighbor;
        }
    }
}

}

template<typename vtype, typename itype>
vtype graph<vtype,itype>::proxl1PRrand(vtype numNodes, vtype* seed, double epsilon, double alpha, double rho, double* q, double* d, double* ds, double* dsinv, double* grad, vtype maxiter)
{
    clock_t timeStamp = clock();
	vtype not_converged = 0;
    vtype Seed = seed[0]; // randomly choose
    double maxNorm;
    grad[Seed] = alpha*dsinv[Seed];  // grad = -gradient
    maxNorm = abs(grad[Seed]*dsinv[Seed]);
    vtype* candidates = new vtype[numNodes];
    bool* visited = new bool[numNodes];
    for (vtype i = 0; i < numNodes; ++i) visited[i] = false;
    vtype candidatesSize = 1;
    candidates[0] = Seed;
    visited[Seed] = true;
    // exp start write graph
    proxl1PRrand::writeGraph(numNodes, "/home/c55hu/Documents/research/experiment/output/graph.txt", ai, aj);
    // proxl1PRrand::writeQdiag(numNodes, "/home/c55hu/Documents/research/experiment/output/Qdiag.txt", ai, aj, d, alpha);
    // exp end
    double threshold = (1+epsilon)*rho*alpha;
    vtype numiter = 1;
    // some constant
    maxiter *= 100;
    double c = (1-alpha)/2;
    double ra = rho*alpha;
    double stepSize = 2.0/(1+alpha);
    for (vtype i = 0; i < numNodes; ++i) ds[i] *= ra;
    while (maxNorm > threshold) {
        vtype r =  proxl1PRrand::getRand() % candidatesSize;
        proxl1PRrand::updateGrad(candidates[r], stepSize, c, ra, q, grad, ds, dsinv, ai, aj, visited, candidates, candidatesSize);
        
        if (numiter % numNodes == 0) {
            maxNorm = 0;
            for (vtype i = 0; i < numNodes; ++i) {
                maxNorm = max(maxNorm, abs(grad[i]*dsinv[i]));
            }
        }
        
        if (numiter++ > maxiter) {
            cout << "not converged" << endl;
            break;
        }
    }
    proxl1PRrand::writeTime(timeStamp, "/home/c55hu/Documents/research/experiment/output/time-rand.txt");
    proxl1PRrand::writeLog(numNodes, "/home/c55hu/Documents/research/experiment/output/q-rand.txt", q);
    // double pSum = 0;
    for (vtype i = 0; i < numNodes; ++i) {
        ds[i] /= ra;
        q[i] = abs(q[i]*ds[i]);
        //pSum += q[i];
    }
    // cout << "total probability: " << pSum << endl;
    delete [] candidates;
    delete [] visited;
    return not_converged;
}

uint32_t proxl1PRrand32(uint32_t n, uint32_t* ai, uint32_t* aj, double* a, double alpha,
                         double rho, uint32_t* v, uint32_t v_nums, double* d, double* ds,
                         double* dsinv, double epsilon, double* grad, double* p, double* y,
                         uint32_t maxiter, uint32_t offset, double max_time)
{
    graph<uint32_t,uint32_t> g(ai[n],n,ai,aj,a,offset,NULL);
    return g.proxl1PRrand(n, v, epsilon, alpha, rho, p, d, ds, dsinv, grad, maxiter);
}

int64_t proxl1PRrand64(int64_t n, int64_t* ai, int64_t* aj, double* a, double alpha,
                        double rho, int64_t* v, int64_t v_nums, double* d, double* ds,
                        double* dsinv,double epsilon, double* grad, double* p, double* y,
                        int64_t maxiter, int64_t offset, double max_time)
{
    graph<int64_t,int64_t> g(ai[n],n,ai,aj,a,offset,NULL);
    return g.proxl1PRrand(n, v, epsilon, alpha, rho, p, d, ds, dsinv, grad, maxiter);
}

uint32_t proxl1PRrand32_64(uint32_t n, int64_t* ai, uint32_t* aj, double* a, double alpha,
                            double rho, uint32_t* v, uint32_t v_nums, double* d, double* ds,
                            double* dsinv, double epsilon, double* grad, double* p, double* y,
                            uint32_t maxiter, uint32_t offset, double max_time)
{
    graph<uint32_t,int64_t> g(ai[n],n,ai,aj,a,offset,NULL);
    return g.proxl1PRrand(n, v, epsilon, alpha, rho, p, d, ds, dsinv, grad, maxiter);
}