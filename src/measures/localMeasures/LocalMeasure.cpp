#include "LocalMeasure.hpp"
#include <vector>
#include <iostream>

using namespace std;

const string LocalMeasure::autogenMatricesFolder = "matrices/autogenerated/";

LocalMeasure::LocalMeasure(Graph* G1, Graph* G2, string name) : Measure(G1, G2, name) {
}

LocalMeasure::~LocalMeasure() {
}

double LocalMeasure::eval(const Alignment& A) {
    uint n = G1->getNumNodes();
    double similaritySum = 0;
    for (uint i = 0; i < n; i++) {
        similaritySum += sims[i][A[i]];
    }
    return similaritySum/n;
}

bool LocalMeasure::isLocal() {
    return true;
}

vector<vector<float> >* LocalMeasure::getSimMatrix() {
    return &sims;
}

void LocalMeasure::loadBinSimMatrix(string simMatrixFileName) {
#if USE_CACHED_FILES
// By default, USE_CACHED_FILES is 0 and SANA does not cache files. Change USE_CACHED_FILES at your own risk.
    if (fileExists(simMatrixFileName)) {
        uint n1 = G1->getNumNodes();
        uint n2 = G2->getNumNodes();
        sims = vector<vector<float> > (n1, vector<float> (n2));
        readMatrixFromBinaryFile(sims, simMatrixFileName);
        return;
    }
#endif
    cout << "Computing " << simMatrixFileName << " ... ";
    Timer T;
    T.start();
    initSimMatrix();
    cout << "Loading binary sim matrix done (" << T.elapsedString() << ")" << endl;
#if USE_CACHED_FILES
// By default, USE_CACHED_FILES is 0 and SANA does not cache files. Change USE_CACHED_FILES at your own risk.
    writeMatrixToBinaryFile(sims, simMatrixFileName);
#endif
}

void LocalMeasure::writeSimsWithNames(string outfile) {
    unordered_map<uint,string> mapG1 = G1->getIndexToNodeNameMap();
    unordered_map<uint,string> mapG2 = G2->getIndexToNodeNameMap();
    uint n1 = G1->getNumNodes();
    uint n2 = G2->getNumNodes();
    ofstream fout(outfile);
    for (uint i = 0; i < n1; i++) {
        for (uint j = 0; j < n2; j++) {
            fout << mapG1[i] << " " << mapG2[j] << " " << sims[i][j] << endl;
        }
    }
}

double LocalMeasure::balanceWeight(){ //outputs the weight this measure should be multiplied by to scale kind of close to 0 through 1
    double totalSim = 0;
    uint simNumber = 0;
    for(uint i = 0; i < sims.size(); i++){
        for(uint j = 0; j < sims[i].size(); j++){
            totalSim += sims[i][j];
            simNumber++;
        }
    }
    double averageSim = totalSim/simNumber;
    return .5/averageSim; //average sim is scaled to one half
}
