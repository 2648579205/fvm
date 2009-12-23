#include <iostream>
#include <fstream>
#include <string>
#include <sstream>
#include "MeshAssembler.h"
#include "CRConnectivity.h"
#include "MultiField.h"

using namespace std;

MeshAssembler::   MeshAssembler( const MeshList& meshList ):_meshList( meshList )
{

    init();
}

MeshAssembler::~MeshAssembler()
{
  
}


void
MeshAssembler::init()
{
  int id = 9;
  int dim = 2;
   //construct merged Linearsystem
  _mesh = new Mesh( dim, id);


  _interfaceNodesSet.resize( _meshList.size() );
  _localInterfaceNodesToGlobalMap.resize( _meshList.size() );

   setCellsSite();
   setFacesSite();
   setInterfaceNodes();
   //countInterfaceNodes();
   setNodesSite();
   setCellsMapper();
   cout << " 1 " << endl;
   setNodesMapper();
cout << " 2 " << endl;
   setFaceCells();
cout << " 3 " << endl;
   setFaceNodes();
cout << " 4 " << endl;
}

//setting storagesite for cells
void 
MeshAssembler::setCellsSite()
{
   int siteCount     = 0;
   int siteSelfCount = 0;
   for ( unsigned int id = 0; id < _meshList.size(); id++ ){
      const StorageSite& site = _meshList[id]->getCells();
      const StorageSite::ScatterMap& scatterMap = site.getScatterMap();
      int nghost = 0;
      foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
         const Array<int>& fromIndices = *(mpos.second);
         nghost += fromIndices.getLength();
      }
      siteCount += site.getCount() - nghost;
      siteSelfCount += site.getSelfCount();
   }

   _cellSite = StorageSitePtr( new StorageSite(siteSelfCount, siteCount-siteSelfCount) );

}

//setting storagesite for faces
void 
MeshAssembler::setFacesSite()
{
   int faceCount = 0;
   for ( unsigned int id = 0; id < _meshList.size(); id++ ){
      const StorageSite& site = _meshList[id]->getFaces();
      faceCount += site.getCount();
   }

   int sharedFaceCount = 0;
   for ( unsigned int id = 0; id < _meshList.size(); id++ ){
        const FaceGroupList &  faceGroupList = _meshList[id]->getInterfaceGroups();
       for ( int n = 0; n < _meshList[id]->getInterfaceGroupCount(); n++ ){
          const StorageSite& site = faceGroupList[n]->site;
          sharedFaceCount += site.getCount();
       }
   }
   _faceSite = StorageSitePtr( new StorageSite(faceCount - sharedFaceCount / 2 ) );
    assert( sharedFaceCount%2  == 0 );

}


void
MeshAssembler::setInterfaceNodes()
{
   //loop over meshes
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));
      const CRConnectivity& faceNodes = mesh.getAllFaceNodes();
      const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();
      //looop over  interfaces
      for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           //loop over faces to fill nodes
           const StorageSite& site = faceGroupList[i]->site;
           int jstart =   site.getOffset(); 
           int jend   = jstart + site.getCount();
           for ( int face = jstart; face < jend; face++ ){
               //loop over face surronding nodes
               for ( int node = 0; node < faceNodes.getCount(face); node++ )
                   nodes.insert( faceNodes(face,node) );
           }
      }
   }

}

//this method counts interface nodes after merging, 
//it seems only way to match is using its coordinates 
void 
MeshAssembler::countInterfaceNodes()
{
   
  for ( unsigned int n = 0; n < _meshList.size(); n++ ){
     const Mesh& mesh = *(_meshList.at(n));
     const StorageSite& faceSite = mesh.getFaces();
     const CRConnectivity& cellFaces = mesh.getCellFaces();
     for ( int i = 0; i < faceSite.getCount(); i++ ){
         cout << " cellFaces[" << i << " ] = ";
         for ( int j = 0; j < cellFaces.getCount(i); j++ )
             cout << "  " << cellFaces(i,j);
         cout << endl;
     }
  }
  //writing mapping
  for ( unsigned int i = 0; i < _meshList.size(); i++ ){
       const StorageSite& thisSite = _meshList[i]->getCells();
       const StorageSite::ScatterMap& scatterMap = thisSite.getScatterMap();
       foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
          const StorageSite& oSite    = *(mpos.first);
          const Array<int>& fromIndices = *(mpos.second);
          const Array<int>& toIndices   = *(oSite.getGatherMap().find(&thisSite)->second);
          cout << " fromIndices.getLength() = " << fromIndices.getLength() << endl;
          cout << " scatterArray " << endl;
          for ( int n  = 0; n < fromIndices.getLength(); n++){
             cout << "      fromIndices[" << n << "] = " << fromIndices[n] << "      toIndices[" << n << "] = " << toIndices[n] << endl;
          }
       }
   }

}

//setting Storage site for nodes
void 
MeshAssembler::setNodesSite()
{
  //count inner nodes for assembly
  int nodeCount       = getInnerNodesCount();
  int nInterfaceNodes = getInterfaceNodesCount();
  _nodeSite = StorageSitePtr( new StorageSite(nodeCount + nInterfaceNodes) );

}

//gettin localToGlobal and globalToLocal for cell
void
MeshAssembler::setCellsMapper()
{
    //count cells
    int selfCount = 0;
    for ( unsigned int id = 0; id < _meshList.size(); id++ ){
       const StorageSite& cellSite = _meshList[id]->getCells();
       selfCount += cellSite.getSelfCount();
    }
    _globalCellToMeshID.resize( selfCount );
    _globalCellToLocal.resize ( selfCount );
    //loop over meshes
    int glblIndx = 0;
    for ( unsigned int id = 0; id < _meshList.size(); id++ ){
       const StorageSite& cellSite = _meshList[id]->getCells();
       _localCellToGlobal[id]   = ArrayIntPtr( new Array<int>( cellSite.getCount() ) ); 
        Array<int>&  localToGlobal = *_localCellToGlobal[id];
        localToGlobal = -1; //initializer 
       //loop over inner cells
       for ( int n = 0; n < cellSite.getSelfCount(); n++ ){
            localToGlobal[n] = glblIndx;
           _globalCellToMeshID[glblIndx] = id;  //belongs to which mesh from global Cell ID
           _globalCellToLocal [glblIndx] = n;   //gives local Cell ID from Global ID but which mesh comes from mapGlobalCellToMeshID
            glblIndx++;
       }
    }

  //creating cellID MultiField to use sync() operation
  shared_ptr<MultiField>  cellMultiField = shared_ptr<MultiField>( new MultiField()    );
  shared_ptr<Field>       cellField      = shared_ptr<Field>     ( new Field("cellID") );
 
  for ( unsigned int n = 0; n < _meshList.size(); n++ ){
     const StorageSite* site = &_meshList[n]->getCells();
     MultiField::ArrayIndex ai( cellField.get(), site );
     shared_ptr<Array<int> > cIndex(new Array<int>(site->getCount()));
      *cIndex = -1;
     cellMultiField->addArray(ai,cIndex);
  }

  //fill  local mesh with global Indx
  for ( unsigned int n = 0; n < _meshList.size(); n++ ){
     const StorageSite* site = &_meshList[n]->getCells();
     MultiField::ArrayIndex ai( cellField.get(), site );
     Array<int>&  localCell = dynamic_cast< Array<int>& >( (*cellMultiField)[ai] ); 
     const Array<int>&  localToGlobal = *_localCellToGlobal[n];
     for ( int i = 0; i < site->getSelfCount(); i++ )
          localCell[i] =localToGlobal[i];
  }
  //sync opeartion
  cellMultiField->sync();

  //fill  local mesh with global Indx after sync
  for ( unsigned int n = 0; n < _meshList.size(); n++ ){
     const StorageSite* site = &_meshList[n]->getCells();
     MultiField::ArrayIndex ai( cellField.get(), site );
     const Array<int>&  localCell = dynamic_cast< const Array<int>& >( (*cellMultiField)[ai] ); 
     Array<int>&  localToGlobal = *_localCellToGlobal[n];
     for ( int i = 0; i < site->getCount(); i++ )
          localToGlobal[i] = localCell[i];
  }


//    //above algorithm fill localCellToGlobal for only inner cells but we will do ghost cells for interfaces
//    for ( unsigned int i = 0; i < _meshList.size(); i++ ){
//        const StorageSite& thisSite = _meshList[i]->getCells();
//        const StorageSite::ScatterMap& scatterMap = thisSite.getScatterMap();
//        foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
//           const StorageSite& oSite    = *(mpos.first);
//           const Array<int>& fromIndices = *(mpos.second);
//           const Array<int>& toIndices   = *(oSite.getGatherMap().find(&thisSite)->second);
//           cout << " fromIndices.getLength() = " << fromIndices.getLength() << endl;
//           cout << " scatterArray " << endl;
//           for ( int n  = 0; n < fromIndices.getLength(); n++){
//              cout << "      fromIndices[" << n << "] = " << fromIndices[n] << "      toIndices[" << n << "] = " << toIndices[n] << endl;
//           }
//        }
//    }

}

//getting CRConnectivity faceCells
void
MeshAssembler::setFaceCells()
{
    _faceCells = CRConnectivityPtr( new CRConnectivity( *_faceSite, *_cellSite) );
    _faceCells->initCount();
    
     //addCount
     const int cellCount = 2;
     for ( int i = 0; i < _faceSite->getCount(); i++ )
        _faceCells->addCount(i, cellCount); // face has always two cells
     //finish count
     _faceCells->finishCount();

     //first interior faces 
     int face = 0;
     for ( unsigned int n = 0; n < _meshList.size(); n++ ){
        const Mesh& mesh = *(_meshList[n]);
        const CRConnectivity& faceCells = mesh.getAllFaceCells();
        const FaceGroup& faceGroup = mesh.getInteriorFaceGroup();
        const Array<int>& localToGlobal = *_localCellToGlobal[n];
        for ( int i = 0; i < faceGroup.site.getCount(); i++ ){
            int cell1 = faceCells(i,0);
            int cell2 = faceCells(i,1);
            _faceCells->add( face, localToGlobal[ cell1 ] );
            _faceCells->add( face, localToGlobal[ cell2 ] );
            face++;
        }
     }

     //now add interior faces
     set<int> faceGroupSet; // this set make sure that no duplication in sweep
     for ( unsigned int n = 0; n < _meshList.size(); n++ ){
          const Mesh& mesh = *(_meshList[n]);
          const CRConnectivity& faceCells = mesh.getAllFaceCells();
          const FaceGroupList&  faceGroupList = mesh.getInterfaceGroups();
          const Array<int>& localToGlobal = *_localCellToGlobal[n];
          //loop over interfaces 
          for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
               int faceGroupID = faceGroupList[i]->id;
               //pass only if it is not in faceGroupSet
               if ( faceGroupSet.count( faceGroupID) == 0 ){
                   faceGroupSet.insert( faceGroupID );
                   //sweep interfaces for add up operation
                   int ibeg = faceGroupList[i]->site.getOffset();
                   int iend = ibeg + faceGroupList[i]->site.getCount();
                   for ( int i = ibeg; i < iend; i++ ){
                      int cell1 = faceCells(i,0);
                      int cell2 = faceCells(i,1);
                      _faceCells->add( face, localToGlobal[ cell1 ] );
                      _faceCells->add( face, localToGlobal[ cell2 ] );
                      face++;
                   }
               }
           }
        cout << endl;
      }
     //now add boundary faces
     for ( unsigned int n = 0; n < _meshList.size(); n++ ){
          const Mesh& mesh = *(_meshList[n]);
          const CRConnectivity& faceCells = mesh.getAllFaceCells();
          const FaceGroupList&  bounGroupList = mesh.getBoundaryFaceGroups();
          const Array<int>& localToGlobal = *_localCellToGlobal[n];
          //loop over interfaces 
          for ( int i = 0; i <mesh.getBoundaryGroupCount(); i++ ){
              //sweep interfaces for add up operation
              int ibeg = bounGroupList[i]->site.getOffset();
              int iend = ibeg + bounGroupList[i]->site.getCount();
              for ( int i = ibeg; i < iend; i++ ){
                  int cell1 = faceCells(i,0);
                  int cell2 = faceCells(i,1);
                  //cell1
                  if ( cell1 < _cellSite->getSelfCount() )
                      _faceCells->add( face, localToGlobal[ cell1 ] ); 
                  else 
                      _faceCells->add( face, cell1  );  //use original boundary cell ID
                  //cell2
                  if ( cell2 < _cellSite->getSelfCount() )
                      _faceCells->add( face, localToGlobal[ cell2 ] ); 
                  else 
                       _faceCells->add( face, cell2 );  //use original boundary cell ID
                  face++;
              }
          }
       }

      //finishAdd
      _faceCells->finishAdd();


}

//getting CRConnectivity faceNodes
void
MeshAssembler::setFaceNodes()
{
    _faceNodes = CRConnectivityPtr( new CRConnectivity( *_faceSite, *_nodeSite) );
    _faceNodes->initCount();
     //addCount
     const Mesh& mesh0 = *_meshList[0];
     const int nodeCount = mesh0.getAllFaceNodes().getRow()[1] -  mesh0.getAllFaceNodes().getRow()[0];
     for ( int i = 0; i < _faceSite->getCount(); i++ )
        _faceNodes->addCount(i, nodeCount); 
     //finish count
     _faceNodes->finishCount();
     //first interior faces 
     int face = 0;
     for ( unsigned int n = 0; n < _meshList.size(); n++ ){
        const Mesh& mesh = *(_meshList[n]);
        const StorageSite& faceSite = mesh.getFaces();
        const CRConnectivity& faceNodes = mesh.getAllFaceNodes();
        const Array<int>& localToGlobal = *_localNodeToGlobal[n];
        cout << " size = " << localToGlobal.getLength() << endl;
        for ( int i = 0; i < faceSite.getCount(); i++ ){
           for ( int j = 0; j < faceNodes.getCount(i); j++ ){
              int nodeID = faceNodes(i,j);
              cout << " n = " << n << " nodeID = " << nodeID << endl;
              _faceNodes->add( face, localToGlobal[nodeID] );
           }
           face++;
        }
     }
    //finishAdd
     _faceNodes->finishAdd();
}

//count only inner nodes of all mesh after assembly
int
MeshAssembler::getInnerNodesCount(){  
   int nodeCount = 0;
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));
      const CRConnectivity& faceNodes = mesh.getAllFaceNodes();
//      const FaceGroup& faceGroup = mesh.getInteriorFaceGroup();
      const StorageSite& site = _meshList[n]->getNodes();
      Array<int>  nodeArray( site.getCount() );
      nodeArray = -1;
      //looop over  interior faces
      int nface = mesh.getFaces().getCount();
      for ( int i = 0; i < nface; i++ )
          //loop over face surronding nodes
           for ( int node = 0; node < faceNodes.getCount(i); node++ )
               nodeArray[ faceNodes(i,node) ] = 1;

    //loop over interface faces to reset 
     const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();  
     for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           foreach ( const set<int>::value_type node, nodes )
               nodeArray[ node] = -1;  //reseting again, we want to seperate inner/boundary nodes than interface nodes
     }
     //node count
     for ( int i = 0; i < nodeArray.getLength(); i++ )
         if ( nodeArray[i] != -1 ) nodeCount++;
   }

  return nodeCount;
}

//count nodes (duplicated) on shared interfaces from each local mesh side
int
MeshAssembler::getInterfaceNodesDuplicatedCount()
{
 //loop over meshes
   int nInterfaceNodes = 0;
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));
      const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();
      //looop over  interfaces
      for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           nInterfaceNodes += nodes.size();
      }
   }
   return nInterfaceNodes;
}

//count nodes on interfaces (not duplicated) 
int
MeshAssembler::getInterfaceNodesCount()
{
  //count nodes on shared interfaces (duplicated)
  int nInterfaceNodes = getInterfaceNodesDuplicatedCount();

   Array< Mesh::VecD3 > interfaceNodeValues ( nInterfaceNodes );
   Array< int > globalIndx  ( nInterfaceNodes );
   globalIndx   = -1;

   //filing interfaceNodeValues
   int indx = 0;
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));
      const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();  
      const Array<Mesh::VecD3>& coord = mesh.getNodeCoordinates();
      //looop over  interfaces
      for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           foreach ( const set<int>::value_type node, nodes ){
              interfaceNodeValues[indx] = coord[node];
              indx++;
           }
      }
   }

   //greedy algorithm to fill interfaceNodeCount
  indx =0;
  for ( int i = 0; i < nInterfaceNodes; i++ ){
     if ( globalIndx[i] == -1 ){
        globalIndx[i] = indx;
        double x = interfaceNodeValues[i][0];
        double y = interfaceNodeValues[i][1];
        double z = interfaceNodeValues[i][2];
        for ( int j = i+1; j < nInterfaceNodes; j++ ){
           double xOther = interfaceNodeValues[j][0];
           double yOther = interfaceNodeValues[j][1];
           double zOther = interfaceNodeValues[j][2];
           if ( x == xOther && y == yOther && z == zOther )
              globalIndx[j]         = indx;
        }
        indx++;
     }
  }
  _nInterfaceNodes = indx;
  //filling localInterfaceNodes to GlobalNodes data structure
   indx = 0;
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));
      const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();
      //looop over  interfaces
      for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           foreach ( const set<int>::value_type node, nodes ){
              _localInterfaceNodesToGlobalMap[n][node] = globalIndx[indx];
              indx++;
           }
      }
   }

   return _nInterfaceNodes;
}

//local Nodes to global Nodes 
void
MeshAssembler::setNodesMapper()
{

    int glblIndx = _nInterfaceNodes; // node numbering first from interfaces
    for ( unsigned int id = 0; id < _meshList.size(); id++ ){
       const StorageSite& nodeSite = _meshList[id]->getNodes();
       cout << " nodeSite .getLength() = " << nodeSite.getSelfCount() << endl;
       _localNodeToGlobal[id]   = ArrayIntPtr( new Array<int>( nodeSite.getCount() ) ); 
        const map<int,int>&  interfaceNodesMap = _localInterfaceNodesToGlobalMap[id];
        Array<int>&  localToGlobal = *_localNodeToGlobal[id];
        localToGlobal = -1; //initializer 
        //loop over inner cells
        for ( int n = 0; n < nodeSite.getCount(); n++ ){
            if ( interfaceNodesMap.count(n) == 0 ) //if it is not in the map, it is inner nodes
               localToGlobal[n] = glblIndx++;
            else                                  // else get from interfaceNodesMap
               localToGlobal[n] = interfaceNodesMap.find(n)->second;
        }
    }

}

void  
MeshAssembler::debug_print()
{
   
  stringstream ss;
  ss << "meshAssembler_debug.dat";
  ofstream  debug_file( (ss.str()).c_str() );
  
  for ( unsigned int i = 0; i < _meshList.size(); i++ ){
     debug_file <<  " mesh  = " << i << ", id  = " << _meshList[i]->getID() << endl;
     debug_file <<  " mesh  = " << i << ", dim = " << _meshList[i]->getDimension() << endl;
  }

  debug_file << endl;

  for ( unsigned int i = 0; i < _meshList.size(); i++ ){
      const StorageSite& cells = _meshList[i]->getCells();
      debug_file << " cells.getSelfCount()  = " << cells.getSelfCount() << " cells.getCount() = " << cells.getCount() << endl;
  }

  debug_file << endl;
  //writing mapping
  for ( unsigned int i = 0; i < _meshList.size(); i++ ){
       const StorageSite& cells = _meshList[i]->getCells();
       const StorageSite::ScatterMap& scatterMap = cells.getScatterMap();
       foreach(const StorageSite::ScatterMap::value_type& mpos, scatterMap){
          const Array<int>& fromIndices = *(mpos.second);
          debug_file << " fromIndices.getLength() = " << fromIndices.getLength() << endl;
          debug_file << " scatterArray " << endl;
          for ( int n  = 0; n < fromIndices.getLength(); n++)
             debug_file << "      fromIndices[" << n << "] = " << fromIndices[n] << endl;
       }
   }
   debug_file << endl;

    //dumping _interfaceNodesSet
 //loop over meshes
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      const Mesh& mesh = *(_meshList.at(n));     
      const FaceGroupList& faceGroupList = mesh.getInterfaceGroups();
      debug_file << " Mesh =  " << n << endl;
      //looop over  interfaces
      for ( int i = 0; i < mesh.getInterfaceGroupCount(); i++ ){
           int id = faceGroupList[i]->id;
           set<int>&  nodes   = _interfaceNodesSet[n][id];
           debug_file << "         face : " << id << endl;
           foreach ( const set<int>::value_type node, nodes )
                debug_file <<  "           " << node << endl;
      }
   }
   debug_file << endl;

   debug_file << " cells.getSelfCount() = " << _cellSite->getSelfCount() << " cells.selfCount() = " << _cellSite->getCount() << endl;
   debug_file << endl; 
   debug_file << " faces.getSelfCount() = " << _faceSite->getSelfCount() << " faces.selfCount() = " << _faceSite->getCount() << endl;
   debug_file << endl; 
   debug_file << " nodes.getSelfCount() = " << _nodeSite->getSelfCount() << " nodes.selfCount() = " << _nodeSite->getCount() << endl;
   debug_file << endl; 
  //mapLocalToGlobal
   for ( unsigned int n = 0; n < _meshList.size(); n++ ){
      debug_file << " mesh = " << n << endl;
      const Array<int>& localToGlobal = *_localCellToGlobal[n];
      for ( int i = 0; i < localToGlobal.getLength(); i++ )
          debug_file  << " localCellToGlobal[" << i << "] = " << localToGlobal[i] << endl;
      debug_file << endl;
   }
   debug_file << endl;
   //globalCellToMeshID
   for (  unsigned int i = 0; i < _globalCellToMeshID.size(); i++ )
      debug_file  << " globalCellToMeshID[" << i << "] = " << _globalCellToMeshID[i] << endl;
  
   debug_file << endl;
   //globalCellToLocal
   for ( unsigned int i = 0; i < _globalCellToLocal.size(); i++ )
      debug_file  << " globalCellToLocal[" << i << "] = " << _globalCellToLocal[i] << endl;
    
   debug_file << endl;


  //printing things
  debug_file << " localCellToGlobal after sync() opeartion " << endl;
  for ( unsigned int n = 0; n < _meshList.size(); n++ ){
     Array<int>&  localToGlobal = *_localCellToGlobal[n];
     debug_file << " mesh = " << n << endl;
     for ( int i = 0; i < localToGlobal.getLength(); i++ )
         debug_file << " localToGlobal[" << i << "] = " << localToGlobal[i] << endl;
     debug_file << endl;
  }
  debug_file << endl;
  //faceCells
  debug_file << " faceCells Connectivity " << endl;
  for ( int i = 0; i < _faceSite->getCount(); i++ ) { 
      int ncells = _faceCells->getCount(i);
      for ( int j = 0; j < ncells; j++ ){
         debug_file << " faceCells(" << i << "," << j << ") = " << (*_faceCells)(i,j);
      }
      debug_file << endl;
  }
  

  debug_file.close();

}