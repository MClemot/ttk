/// \ingroup base
/// \class ttk::ContourTreeAlignment
/// \author Florian Wetzels (f_wetzels13@cs.uni-kl.de), Jonas Lukasczyk <jl@jluk.de>
/// \date 31.01.2020
///
/// \brief TTK %contourTreeAlignment processing package.
///
/// %ContourTreeAlignment is a TTK processing package that TODO
///
/// \sa ttk::Triangulation

#pragma once

// base code includes
#include <Debug.h>
#include <random>
#include <algorithm>
#include "contourtree.h"

enum Type_Alignmenttree { averageValues, medianValues, lastMatchedValue };
//enum Type_Match { matchNodes, matchArcs };
enum Mode_ArcMatch { persistence, area, volume };

struct AlignmentTree{
    AlignmentTree* child1;
    AlignmentTree* child2;
    BinaryTree* node1;
    BinaryTree* node2;
    int size;
    int height;
    //int freq;
};

struct AlignmentEdge;

struct AlignmentNode{

    Type_Node type;
    int freq;
    float scalarValue;
    int branchID;

    std::vector<AlignmentEdge*> edgeList;

    std::vector<std::pair<int,int>> nodeRefs;

};

struct AlignmentEdge{

    AlignmentNode* node1;
    AlignmentNode* node2;
    float scalardistance;
    float area;
    float volume;
    int freq;

    std::vector<std::pair<int,int>> arcRefs;

};

using namespace std;

namespace ttk{

    class ContourTreeAlignment : virtual public Debug{

    public:

        /// constructor and destructor
        ContourTreeAlignment(){
            this->setDebugMsgPrefix("ContourTreeAlignment");
        };
        ~ContourTreeAlignment(){};

        /// setter for parameters
        void setArcMatchMode(int mode){
            arcMatchMode = static_cast<Mode_ArcMatch>(mode);
        }
        void setWeightCombinatorialMatch(float weight){
            weightCombinatorialMatch = weight;
        }
        void setWeightArcMatch(float weight){
            weightArcMatch = weight;
        }
        void setWeightScalarValueMatch(float weight){
            weightScalarValueMatch = weight;
        }
        void setAlignmenttreeType(int type){
            alignmenttreeType = static_cast<Type_Alignmenttree >(type);
        }

        /// The actual iterated n-tree-alignment algorithm
        template <class scalarType> int execute(
            const vector<void*>& scalarsVP,
            const vector<int*>& regionSizes,
            const vector<int*>& segmentationIds,
            const vector<long long*>& topologies,
            const vector<size_t>& nVertices,
            const vector<size_t>& nEdges,

            vector<float>& outputVertices,
            vector<long long>& outputFrequencies,
            vector<long long>& outputVertexIds,
            vector<long long>& outputBranchIds,
            vector<long long>& outputSegmentationIds,
            vector<long long>& outputArcIds,
            vector<int>& outputEdges,
            int seed
        );

        /// functions for aligning single trees in iteration
        bool alignTree(ContourTree* t);
        bool initialize(ContourTree* t);
        bool alignTree_consistentRoot(ContourTree* t);
        bool initialize_consistentRoot(ContourTree* t, int rootIdx);

        /// getters for graph data structures
        std::vector<std::pair<std::vector<CTNode*>,std::vector<CTEdge*>>> getGraphs();
        std::vector<ContourTree*> getContourTrees();

        std::pair<std::vector<AlignmentNode*>,std::vector<AlignmentEdge*>> getAlignmentGraph();
        BinaryTree* getAlignmentGraphRooted();
        int getAlignmentRootIdx();

        /// function for aligning two sarbitrary binary trees
        std::pair<float,AlignmentTree*> getAlignmentBinary( BinaryTree* t1, BinaryTree* t2 );

        /// function that adds branch decomposition information
        void computeBranches();

        static void deleteAlignmentTree(AlignmentTree* t){
            if(t->child1) deleteAlignmentTree(t->child1);
            if(t->child2) deleteAlignmentTree(t->child2);
            delete t;
        }

    protected:

        /// filter parameters
        Type_Alignmenttree alignmenttreeType = averageValues;
        Mode_ArcMatch arcMatchMode = persistence;
        float weightArcMatch = 1;
        float weightCombinatorialMatch = 0;
        float weightScalarValueMatch = 0;

        /// alignment graph data
        std::vector<AlignmentNode*> nodes;
        std::vector<AlignmentEdge*> arcs;

        /// iteration variables
        std::vector<ContourTree*> contourtrees;
        std::vector<size_t> permutation;
        AlignmentNode* alignmentRoot;
        int alignmentRootIdx;
        float alignmentVal;

        /// functions for aligning two trees (computing the alignment value and memoization matrix)
        float alignTreeBinary( BinaryTree* t1, BinaryTree* t2, float** memT, float** memF );
        float alignForestBinary( BinaryTree* t1, BinaryTree* t2, float** memT, float** memF );

        /// functions for the traceback of the alignment computation (computing the actual alignment tree)
        AlignmentTree* traceAlignmentTree(BinaryTree* t1, BinaryTree* t2, float** memT, float** memF);
        std::vector<AlignmentTree*> traceAlignmentForest(BinaryTree* t1, BinaryTree* t2, float** memT, float** memF);
        AlignmentTree* traceNullAlignment(BinaryTree* t, bool first);

        /// function that defines the local editing costs of two nodes
        float editCost( BinaryTree* t1, BinaryTree* t2 );

        /// helper functions for tree data structures
        bool isBinary( Tree* t );
        BinaryTree* rootAtNode(AlignmentNode* root);
        BinaryTree* computeRootedTree(AlignmentNode* node, AlignmentEdge* parent, int &id);
        BinaryTree* computeRootedDualTree(AlignmentEdge* arc, bool parent1, int &id);
        void computeNewAlignmenttree(AlignmentTree* res);

        /// helper functions for branch decomposition
        std::pair<float,std::vector<AlignmentNode*>> pathToMax(AlignmentNode* root, AlignmentNode* parent);
        std::pair<float,std::vector<AlignmentNode*>> pathToMin(AlignmentNode* root, AlignmentNode* parent);

    };
}

template <class scalarType> int ttk::ContourTreeAlignment::execute(
    const vector<void*>& scalarsVP,
    const vector<int*>& regionSizes,
    const vector<int*>& segmentationIds,
    const vector<long long*>& topologies,
    const vector<size_t>& nVertices,
    const vector<size_t>& nEdges,
    vector<float>& outputVertices,
    vector<long long>& outputFrequencies,
    vector<long long>& outputVertexIds,
    vector<long long>& outputBranchIds,
    vector<long long>& outputSegmentationIds,
    vector<long long>& outputArcIds,
    vector<int>& outputEdges,
    int seed
){

    Timer timer;

    size_t nTrees = nVertices.size();

    vector<float*> scalars(nTrees);
    for(size_t t=0; t<nTrees; t++){
        scalars[t] = (float*)((scalarType*)scalarsVP[t]);
    }

    // Print Input
    {
        this->printMsg(ttk::debug::Separator::L1); // L1 is the '=' separator
        this->printMsg("Execute");
        this->printMsg("Computing Alignment for " + std::to_string(nTrees) + " trees.");
        //this->printMsg("Computing Alignment for " + std::to_string(nTrees) + " trees.", 0, t.getElapsedTime(), 1);
    }

    for(size_t t=0; t<nTrees; t++){
        this->printMsg(ttk::debug::Separator::L2,debug::Priority::DETAIL);
        this->printMsg("Tree " + std::to_string(t) + " (cellDimension, vertexId0, vertexId1, scalarOfVertexId0, scalarOfVertexId1, regionSize, segmentationId)",debug::Priority::DETAIL);

        for(size_t i=0; i<nEdges[t]; i++){
            //long long cellDimension = topologies[t][i*3];
            long long vertexId0     = topologies[t][i*2+0];
            long long vertexId1     = topologies[t][i*2+1];
            int regionSize          = regionSizes[t][i];
            int segmentationId      = segmentationIds[t][i];
            scalarType scalarOfVertexId0 = scalars[t][ vertexId0 ];
            scalarType scalarOfVertexId1 = scalars[t][ vertexId1 ];

            this->printMsg(std::to_string(2)//cellDimension)
                + ", " + std::to_string(vertexId0)
                + ", " + std::to_string(vertexId1)
                + ", " + std::to_string(scalarOfVertexId0)
                + ", " + std::to_string(scalarOfVertexId1)
                + ", " + std::to_string(regionSize)
                + ", " + std::to_string(segmentationId),debug::Priority::DETAIL);
        }
    }

    // prepare data structures
    contourtrees = std::vector<ContourTree*>();
    nodes = std::vector<AlignmentNode*>();
    arcs = std::vector<AlignmentEdge*>();
    int bestRootIdx;

    // permute list input trees randomly with given seed
    permutation = std::vector<size_t>();
    for(size_t i=0; i<nTrees; i++){
        permutation.push_back(i);
    }
    std::srand(seed);
    if(alignmenttreeType!=lastMatchedValue) std::random_shuffle(permutation.begin(),permutation.end());

    // print permutation ToDo: optimize for output not necessary?
    std::string permutationString = "";
    for(size_t i=0; i<permutation.size(); i++){
        permutationString += std::to_string(permutation[i]) + (i==permutation.size()-1?"":",");
    }
    this->printMsg(ttk::debug::Separator::L1);
    this->printMsg("Seed for permutation: " + std::to_string(seed),debug::Priority::DETAIL);
    this->printMsg("Permutation: "+permutationString,debug::Priority::DETAIL);
    this->printMsg("Starting alignment heuristic.");

    std::tuple<std::vector<AlignmentNode*>,std::vector<AlignmentEdge*>,std::vector<ContourTree*>> bestAlignment;
    float bestAlignmentValue = FLT_MAX;

    for(int rootIdx=0; rootIdx<nVertices[permutation[0]]; rootIdx++){
    //for(int rootIdx=2; rootIdx<3; rootIdx++){

        contourtrees.clear();
        nodes.clear();
        arcs.clear();
        alignmentVal = 0;


        this->printMsg(ttk::debug::Separator::L2);
        this->printMsg("Alignment computation started with root " + std::to_string(rootIdx));

        // initialize alignment with first tree

        bool binary;
        bool init = false;
        size_t i=0;
        while(i<nTrees){

            ContourTree* ct = new ContourTree(scalars[permutation[i]],
                    regionSizes[permutation[i]],
                    segmentationIds[permutation[i]],
                    topologies[permutation[i]],
                    nVertices[permutation[i]],
                    nEdges[permutation[i]]);

            if(ct->getGraph().first.size() != nVertices[permutation[i]]){

                delete ct;
                this->printErr("WTF??");
                return 0;

            }

            binary = initialize_consistentRoot(ct,rootIdx);
            if(binary) init = true;
            else{
                this->printWrn("Input " + std::to_string(permutation[i]) + " not binary.");
                delete ct;
            }
            if(init) break;

            i++;

        }
        if(!init){

            this->printErr("No input binary.");

            return 0;

        }

        this->printMsg("Alignment initialized with tree " + std::to_string(permutation[i]),debug::Priority::DETAIL);

        if(alignmentRoot->type == saddleNode){

            this->printMsg("Initialized root is saddle, alignment aborted.");

            for(AlignmentNode* node : nodes){
                delete node;
            }
            for(AlignmentEdge* edge : arcs){
                delete edge;
            }
            for(ContourTree* ct : contourtrees){
                delete ct;
            }

            continue;

        }

        // construct other contour tree objects and align them

        i++;
        while(i<nTrees){

            ContourTree* ct = new ContourTree(scalars[permutation[i]],
                                              regionSizes[permutation[i]],
                                              segmentationIds[permutation[i]],
                                              topologies[permutation[i]],
                                              nVertices[permutation[i]],
                                              nEdges[permutation[i]]);

            binary = alignTree_consistentRoot(ct);
            if(!binary){
                this->printWrn("Input " + std::to_string(permutation[i]) + " not binary.");
                delete ct;
            }
            else this->printMsg("Tree " + std::to_string(permutation[i]) + " aligned.",debug::Priority::DETAIL);

            i++;

        }

        this->printMsg("All trees aligned. Total alignment value: " + std::to_string(alignmentVal));

        if(alignmentVal < bestAlignmentValue){

            for(AlignmentNode* node : std::get<0>(bestAlignment)){
                delete node;
            }
            for(AlignmentEdge* edge : std::get<1>(bestAlignment)){
                delete edge;
            }
            for(ContourTree* ct : std::get<2>(bestAlignment)){
                delete ct;
            }

            bestAlignmentValue = alignmentVal;
            bestAlignment = std::make_tuple(nodes,arcs,contourtrees);
            bestRootIdx = rootIdx;

        }
        else{

            for(AlignmentNode* node : nodes){
                delete node;
            }
            for(AlignmentEdge* edge : arcs){
                delete edge;
            }
            for(ContourTree* ct : contourtrees){
                delete ct;
            }

        }

    }

    nodes = std::get<0>(bestAlignment);
    arcs = std::get<1>(bestAlignment);
    contourtrees = std::get<2>(bestAlignment);

    this->printMsg(ttk::debug::Separator::L1);
    this->printMsg("Alignment iteration complete. Root of optimal alignment: " + std::to_string(bestRootIdx) + ".");
    this->printMsg(ttk::debug::Separator::L1);
    this->printMsg("Computing branches.");

    computeBranches();

    this->printMsg("Branches computed.");
    this->printMsg(ttk::debug::Separator::L1);
    this->printMsg("Writing output.");

    for(AlignmentNode* node : nodes){

        outputVertices.push_back(node->scalarValue);
        outputFrequencies.push_back(node->freq);
        outputBranchIds.push_back(node->branchID);
        std::vector<long long> refs(nTrees,-1);
        std::vector<long long> segRefs(nTrees,-1);
        for(std::pair<int,int> ref : node->nodeRefs){
            refs[permutation[ref.first]] = ref.second;
            int eId = contourtrees[ref.first]->getGraph().first[ref.second]->edgeList[0];
            segRefs[permutation[ref.first]] = contourtrees[ref.first]->getGraph().second[eId]->segId;
        }
        for(int ref : refs){
            outputVertexIds.push_back(ref);
        }
        for(int segRef : segRefs){
            outputSegmentationIds.push_back(segRef);
        }

    }

    for(AlignmentEdge* edge : arcs){

        int i=0;
        for(AlignmentNode* node : nodes){

            if(node == edge->node1){
                outputEdges.push_back(i);
            }

            if(node == edge->node2){
                outputEdges.push_back(i);
            }

            i++;

        }

        std::vector<long long> arcRefs(nTrees,-1);
        for(std::pair<int,int> ref : edge->arcRefs){
            arcRefs[permutation[ref.first]] = ref.second;
        }
        for(int ref : arcRefs){
            outputArcIds.push_back(ref);
        }

    }

    // Print performance
    {
        this->printMsg(ttk::debug::Separator::L2);
        this->printMsg("Alignment computed in " + std::to_string(timer.getElapsedTime()) + " s. (" + std::to_string(threadNumber_) + " thread(s)).");
        this->printMsg("Number of nodes in alignment: " + std::to_string(nodes.size()));
        this->printMsg(ttk::debug::Separator::L1);
    }

    return 1;
}
