#include <vector>
#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <set>
#include <cassert>
#include <unordered_set>
#include <unordered_map>
#include "GoSimilarity.hpp"

using namespace std;

const string GoSimilarity::biogridGOFile = "go/gene2go";

GoSimilarity::GoSimilarity(Graph* G1, Graph* G2,
    const vector<double>& countWeights, double occurrencesFraction) :
    LocalMeasure(G1, G2, "go"),
    occurrencesFraction(occurrencesFraction) {

    vector<double> normWeights(countWeights);
    normalizeWeights(normWeights);
    this->countWeights = normWeights;

    string fileName = autogenMatricesFolder+G1->getName()+"_"+
        G2->getName()+"_go_"+to_string(normWeights.size());
    for (double w : normWeights)
        fileName += "_" + extractDecimals(w, 3);
    fileName += "_frac_"+extractDecimals(occurrencesFraction, 3);
    fileName += ".bin";

    loadBinSimMatrix(fileName);
}

string GoSimilarity::getGoSimpleFileName(const Graph& G) {
    string name = G.getName();
    string goFile = "networks/"+name+"/go/"+name+"_gene_association.txt";
    if (fileExists(goFile)) {
        return "networks/"+name+"/autogenerated/"+name+"_go_simple.txt";
    }
    else if (isBioGRIDNetwork(G)) {
        return "go/autogenerated/gene2go_simple.txt";
    }
    else {
        throw runtime_error("GO data not available for this network");
        return "";
    }
}

void GoSimilarity::ensureGoFileSimpleFormatExists(const Graph& G) {
    string name = G.getName();
    string goFile = "networks/"+name+"/go/"+name+"_gene_association.txt";
    string GoSimpleFile;
    cout << "Computing " << getGoSimpleFileName(G) << "...";
    Timer T;
    T.start();
#if USE_CACHED_FILES
// By default, USE_CACHED_FILES is 0 and SANA does not cache files. Change USE_CACHED_FILES at your own risk.
    if (fileExists(goFile)) {
        GoSimpleFile = "networks/"+name+"/autogenerated/"+name+"_go_simple.txt";
        if (not fileExists(GoSimpleFile)) {
            GoSimilarity::generateGOFileSimpleFormat(goFile, GoSimpleFile);
        }
        cout << "GO simple format done (" << T.elapsedString() << ")" << endl;
        return;
    }
#endif
    GoSimpleFile = "go/autogenerated/gene2go_simple.txt";
    if (not fileExists(GoSimpleFile)) {
        GoSimilarity::generateGene2GoSimpleFormat();
    }
    
    cout << "GO simple format done (" << T.elapsedString() << ")" << endl;
}

//extracts the relevant columns from a GOFile
//namely, the protein name, the goterm identifier, and the
//Aspect (F Function, P Process or C Component)
void GoSimilarity::generateGOFileSimpleFormat(string GOFile, string GOFileSimpleFormat) {
    ifstream infile(GOFile);
    ofstream outfile(GOFileSimpleFormat);
    string line;
    while (getline(infile, line)) {
        if (line[0] == '!') continue; //header line
        istringstream iss(line);
        string waste, protein;
        iss >> waste >> waste >> protein;
        string goTerm;
        iss >> goTerm;
        while (goTerm[0] != 'G' or goTerm[1] != 'O') {
            //this is necessary because sometimes there is a optional 4th column with qualifiers
            iss >> goTerm;
        }
        assert(goTerm.substr(0,3) == "GO:");
        string aspect;
        iss >> waste >> waste >> aspect;
        while (not (aspect.size() == 1 and
            (aspect == "F" or aspect == "P" or aspect == "C"))) {
            //again necessary because there are optional columns
            iss >> aspect;
        }
        outfile << protein << "\t" << goTerm << "\t" << aspect << endl;
    }
}

void GoSimilarity::generateGene2GoSimpleFormat() {
    if (not fileExists(biogridGOFile)) {
        throw runtime_error("Missing file \""+biogridGOFile+"\". It can be downloaded from \"ftp.ncbi.nlm.nih.gov/gene/DATA/gene2go.gz\"");
    }
    ifstream infile(biogridGOFile);
    if(!folderExists("go/autogenerated"));
        createFolder("go/autogenerated");
    ofstream outfile("go/autogenerated/gene2go_simple.txt");
    string line;
    getline(infile, line); //header
    while (getline(infile, line)) {
        istringstream iss(line);
        string waste, protein, goTerm;
        iss >> waste >> protein >> goTerm;
        outfile << protein << "\t" << goTerm << endl;
    }
}

void GoSimilarity::assertNoRepeatedEntries(const vector<vector<uint> >& goTerms) {
    for (uint i = 0; i < goTerms.size(); i++) {
        for (uint j = 0; j < goTerms[i].size(); j++) {
            for (uint k = j+1; k < goTerms[i].size(); k++) {
                assert(goTerms[i][j] != goTerms[i][k]);
            }
        }
    }
}

//puts the go terms in a fast-to-parse format
//the row i of the file contains the list of goterms
//of the i-th protein
void GoSimilarity::simpleToInternalFormat(const Graph& G, string GOFileSimpleFormat, string GOFileInternalFormat) {
    string GName = G.getName();

    unordered_map<string,uint> aux = G.getNodeNameToIndexMap();
    unordered_map<string,uint> nodeToIndexMap(aux.begin(), aux.end());

    ifstream infile(GOFileSimpleFormat);
    string line;
    uint n = G.getNumNodes();
    vector<vector<uint> > goTerms(n, vector<uint> (0));
    while (getline(infile, line)) {
        istringstream iss(line);
        string protein, goTermString;
        iss >> protein >> goTermString;
        uint goTermNum = stoi(goTermString.substr(3)); //skip the 'GO:' prefix
        if (nodeToIndexMap.count(protein)) { //we ignore proteins in the GO file that do not appear in the network file
            uint index = nodeToIndexMap[protein];
            bool alreadyPresent = false;
            for (uint i = 0; i < goTerms[index].size(); i++) {
                if (goTerms[index][i] == goTermNum) alreadyPresent = true;
            }
            if (not alreadyPresent) { //we ignore repeated entries
                goTerms[nodeToIndexMap[protein]].push_back(goTermNum);
            }
        }
    }
    assertNoRepeatedEntries(goTerms);
    ofstream outfile(GOFileInternalFormat);
    for (uint i = 0; i < n; i++) {
        for (uint j = 0; j < goTerms[i].size(); j++) {
            outfile << goTerms[i][j] << " ";
        }
        outfile << endl;
    }
}

void GoSimilarity::ensureGOFileInternalFormatExists(const Graph& G) {
    string GName = G.getName();
    string GOFileInternalFormat = "networks/"+GName+"/autogenerated/"+GName+"_go_internal.txt";
    if (not fileExists(GOFileInternalFormat)) {
        ensureGoFileSimpleFormatExists(G);
        cout << "Computing " << GOFileInternalFormat << "...";
        Timer T;
        T.start();
        simpleToInternalFormat(G, getGoSimpleFileName(G), GOFileInternalFormat);
        cout << "GO internal format done (" << T.elapsedString() << ")" << endl;
    }
    assert(fileExists(GOFileInternalFormat));
}

vector<vector<uint> > GoSimilarity::loadGOTerms(
    const Graph& G, double occurrencesFraction) {
    string GName = G.getName();
    string GOFileInternalFormat = "networks/"+GName+"/autogenerated/"+GName+"_go_internal.txt";
    ensureGOFileInternalFormatExists(G);
    unordered_set<uint> acceptedTerms;
    if (occurrencesFraction < 1) {
        vector<uint> v = leastFrequentGoTerms(G, occurrencesFraction);
        acceptedTerms = unordered_set<uint>(v.begin(), v.end());
    }

    uint n = G.getNumNodes();
    vector<vector<uint> > goTerms(n, vector<uint> (0));
    uint i = 0;
    ifstream infile(GOFileInternalFormat);
    string line;
    while (getline(infile, line)) {
        istringstream iss(line);
        uint goTerm;
        while (iss >> goTerm) {
            if (occurrencesFraction == 1 or
                acceptedTerms.count(goTerm) == 1) {

                goTerms[i].push_back(goTerm);
            }
        }
        i++;
    }
    return goTerms;
}

void GoSimilarity::initSimMatrix() {
    uint n1 = G1->getNumNodes();
    uint n2 = G2->getNumNodes();
    sims = vector<vector<float> > (n1, vector<float> (n2, 0));

    vector<vector<uint> > G1GOTerms = loadGOTerms(*G1, occurrencesFraction);
    vector<vector<uint> > G2GOTerms = loadGOTerms(*G2, occurrencesFraction);

    uint maxNumEqGoTerms = countWeights.size();
    vector<double> accumulativeWeights(countWeights);
    for (uint i = 1; i < maxNumEqGoTerms; i++) {
        accumulativeWeights[i] += accumulativeWeights[i-1];
    }

    sims = vector<vector<float> > (n1, vector<float> (n2, 0));
    for (uint i = 0; i < n1; i++) {
        for (uint j = 0; j < n2; j++) {
            uint count = 0;
            for (uint k = 0; k < G1GOTerms[i].size() and count < maxNumEqGoTerms; k++) {
                for (uint l = 0; l < G2GOTerms[j].size(); l++) {
                    if (G1GOTerms[i][k] == G2GOTerms[j][l]) {
                        count++;
                        break;
                    }
                }
            }
            if (count == 0) {
                sims[i][j] = 0;
            }
            else {
                sims[i][j] = accumulativeWeights[count-1];
            }
        }
    }
}

GoSimilarity::~GoSimilarity() {
}


uint GoSimilarity::numberAnnotatedProteins(const Graph& G) {
    vector<vector<uint> > terms = loadGOTerms(G);
    uint count = 0;
    for (uint i = 0; i < terms.size(); i++) {
        if (terms[i].size() > 0) count++;
    }
    return count;
}

// Variation of eval which does not consider
// proteins which do not have any GO term
// However, using this messes up SANA's incremental
// evaluation, because it treates all local measures
// equally and this is different

// double GoSimilarity::eval(const Alignment& A) {
//     double similaritySum = 0;
//     for (uint i = 0; i < G1->getNumNodes(); i++) {
//         similaritySum += sims[i][A[i]];
//     }
//     uint n1 = numberAnnotatedProteins(*G1);
//     uint n2 = numberAnnotatedProteins(*G2);
//     return similaritySum/min(n1,n2);
// }


bool countComp(const vector<uint> a, const vector<uint> b) {
   return a[1] < b[1];
}

// 'occurrencesFraction' indicates how many of the most
// common GO terms must be removed
// In praticular, for each network we remove the minimum number
// of GO terms such that not more than 'occurrencesFraction' of the
// total GO term occurrences remain
vector<uint> GoSimilarity::leastFrequentGoTerms(
    const Graph& G, double occurrencesFraction) {

    unordered_map<uint,uint> goCountsG1 = getGoCounts(G);
    vector<vector<uint>> counts(0);
    for (auto idCountPair : goCountsG1) {
        vector<uint> v(2);
        v[0] = idCountPair.first;
        v[1] = idCountPair.second;
        counts.push_back(v);
    }
    sort(counts.begin(), counts.end(), countComp);

    uint totalCount = 0;
    for (uint i = 0; i < counts.size(); i++) {
        totalCount += counts[i][1];
    }

    uint maxCount = floor(occurrencesFraction*totalCount);
    vector<uint> keptTerms(0);
    uint i = 0;
    uint partialCount = 0;
    while (i < counts.size() and partialCount < maxCount) {
        keptTerms.push_back(counts[i][0]);
        partialCount += counts[i][1];
        i += 1;
    }
    // cout << "total go terms: " << counts.size() << endl;
    // cout << "total occurrences: " << totalCount << endl;
    // cout << "kept go terms: " << keptTerms.size() << endl;
    // cout << "kept occurrences: " << partialCount << endl;
    return keptTerms;
}

unordered_map<uint,uint> GoSimilarity::getGoCounts(const Graph& G) {
    unordered_map<uint,uint> res;
    vector<vector<uint> > goTerms = GoSimilarity::loadGOTerms(G);
    for (uint i = 0; i < goTerms.size(); i++) {
        for (uint j = 0; j < goTerms[i].size(); j++) {
            if (res.count(goTerms[i][j]) == 0) {
                res[goTerms[i][j]] = 1;
            }
            else {
                res[goTerms[i][j]]++;
            }
        }
    }
    return res;
}

bool GoSimilarity::isBioGRIDNetwork(const Graph& G) {
    string name = G.getName();
    return true; //name == "RNorvegicus" or name == "SPombe" or name == "CElegans" or
        //name == "MMusculus" or name == "AThaliana" or name == "DMelanogaster" or
        //name == "SCerevisiae" or name == "HSapiens";
}

bool GoSimilarity::hasGOData(const Graph& G) {
    if (isBioGRIDNetwork(G)) {
        return fileExists(biogridGOFile);
    } else {
        string name = G.getName();
        string goFile = "networks/"+name+"/go/"+name+"_gene_association.txt";
        return fileExists(goFile);
    }
}

bool GoSimilarity::fulfillsPrereqs(Graph* G1, Graph* G2) {
    return hasGOData(*G1) and hasGOData(*G2);
}
