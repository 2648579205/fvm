#ifndef _MESH_H_
#define _MESH_H_

#include "Array.h"

#include "StorageSite.h"
#include "Vector.h"
#include "Field.h"
#include "FieldLabel.h"
#include "MPM_Particles.h"


class CRConnectivity;
class GeomFields;

class MPM;

struct FaceGroup
{
  FaceGroup(const int count_,
            const int offset_,
            const StorageSite& parent_,
            const int id_,
            const string& groupType_) :
    site(count_,0,offset_,&parent_),
    id(id_),
    groupType(groupType_)
  {}
  
  const StorageSite site;
  const int id;
  string groupType;
};


typedef shared_ptr<FaceGroup> FaceGroupPtr; 
typedef vector<FaceGroupPtr> FaceGroupList;  

class Mesh 
{
public:

  typedef Vector<double,3> VecD3;
  typedef Array<int> IntArray;
  
  typedef pair<const StorageSite*, const StorageSite*> SSPair;
  typedef map<SSPair,shared_ptr<CRConnectivity> > ConnectivityMap;
  typedef pair<int,int>    PartIDMeshIDPair; //other Partition ID, other MeshID (always)
  typedef map< PartIDMeshIDPair, shared_ptr<StorageSite> > GhostCellSiteMap;

  enum
    {
      CELL_BAR2,
      CELL_TRI3,
      CELL_QUAD4,
      CELL_TETRA4,
      CELL_HEX8,
      CELL_PYRAMID,
      CELL_PRISM,
      CELL_TYPE_MAX
    } CellType;
  

  enum
    {
      IBTYPE_FLUID=-1,
      IBTYPE_BOUNDARY=-2,
      IBTYPE_SOLID=-3,
      IBTYPE_REALBOUNDARY=-4,
      IBTYPE_UNKNOWN=-5
    };
  
  Mesh(const int dimension, const int id);
  Mesh(const int dimension, const int id, const Array<VecD3>&  faceNodesCoord ); 
  
  ~Mesh();

  DEFINE_TYPENAME("Mesh");

  int getDimension() const {return _dimension;}
  int getID() const {return _id;}
  
  const StorageSite& getFaces()   const {return _faces;}
  const StorageSite& getCells()   const {return _cells;}
  const StorageSite& getNodes()   const {return _nodes;}
  const StorageSite& getIBFaces() const {return _ibFaces;}

  const StorageSite* getGhostCellSiteScatter(  const PartIDMeshIDPair& id ) const
  { return _ghostCellSiteScatterMap.find(id)->second.get(); }

  const GhostCellSiteMap& getGhostCellSiteScatterMap() const
  { return _ghostCellSiteScatterMap; }

  const StorageSite* getGhostCellSiteGather( const PartIDMeshIDPair& id ) const
  { return _ghostCellSiteGatherMap.find(id)->second.get(); }

  const GhostCellSiteMap& getGhostCellSiteGatherMap() const
  { return _ghostCellSiteGatherMap; }


  StorageSite& getFaces() {return _faces;}
  StorageSite& getCells() {return _cells;}
  StorageSite& getNodes() {return _nodes;}
  StorageSite& getIBFaces() {return _ibFaces;}
 
  // this should only be used when we know that the connectivity
  // exists, for connectivities that are computed on demand the
  // specific functions below should be used
  
  const CRConnectivity& getConnectivity(const StorageSite& from,
                                        const StorageSite& to) const;

  const CRConnectivity& getAllFaceNodes() const;
  const CRConnectivity& getAllFaceCells() const;
  const CRConnectivity& getCellNodes() const;
  
  const CRConnectivity& getFaceCells(const StorageSite& site) const;
  const CRConnectivity& getFaceNodes(const StorageSite& site) const;
  const CRConnectivity& getCellFaces() const;
  const CRConnectivity& getCellCells() const;
  const CRConnectivity& getCellCells2() const;
  const CRConnectivity& getFaceCells2() const;

  CRConnectivity& getAllFaceCells();
  
  const FaceGroup& getInteriorFaceGroup() const {return *_interiorFaceGroup;}
  
  int getFaceGroupCount() const {return _faceGroups.size();}
  int getBoundaryGroupCount() const {return _boundaryGroups.size();}
  int getInterfaceGroupCount() const {return _interfaceGroups.size();}

  const FaceGroupList& getBoundaryFaceGroups() const
  {return _boundaryGroups;}

  const FaceGroupList& getInterfaceGroups() const
  {return _interfaceGroups;}
  
  const FaceGroupList& getAllFaceGroups() const
  {return _faceGroups;}
  

  const StorageSite& createInteriorFaceGroup(const int size);
  const StorageSite& createInterfaceGroup(const int size,const int offset, 
                                    const int id);
  const StorageSite& createBoundaryFaceGroup(const int size, const int offset, 
                                       const int id, const string& boundaryType);

  void setCoordinates(shared_ptr<Array<VecD3> > x) {_coordinates = x;}
  void setFaceNodes(shared_ptr<CRConnectivity> faceNodes);
  void setFaceCells(shared_ptr<CRConnectivity> faceCells);
  
  void setConnectivity(const StorageSite& rowSite, const StorageSite& colSite,
		       shared_ptr<CRConnectivity> conn);
  void eraseConnectivity(const StorageSite& rowSite,
                         const StorageSite& colSite) const;
		    
  
  shared_ptr<Array<int> > createAndGetBNglobalToLocal() const;
  const ArrayBase& getBNglobalToLocal() const;
  const StorageSite& getBoundaryNodes() const;  

  const Array<VecD3>& getNodeCoordinates() const {return *_coordinates;}
  Array<VecD3>& getNodeCoordinates() {return *_coordinates;}
  //  ArrayBase* getNodeCoordinates() {return &(*_coordinates);}

  void setNumOfAssembleMesh( int nmesh ){ _numOfAssembleMesh = nmesh; }

  //VecD3 getCellCoordinate(const int c) const;

  const Array<int>& getIBFaceList() const;

  Array<int>& getCellColors() { return *_cellColor;}
  const Array<int>& getCellColors() const { return *_cellColor;}

  Array<int>& getCellColorsOther() { return *_cellColorOther;}
  const Array<int>& getCellColorsOther() const { return *_cellColorOther;}

  bool isMergedMesh() const { return _isAssembleMesh;}
  int  getNumOfAssembleMesh() const { return _numOfAssembleMesh;}
 

  void setIBFaces(shared_ptr<Array<int> > faceList) {_ibFaceList = faceList;}

  void createGhostCellSiteScatter( const PartIDMeshIDPair& id, shared_ptr<StorageSite> site ); 
  void createGhostCellSiteGather ( const PartIDMeshIDPair& id, shared_ptr<StorageSite> site ); 
  void createCellColor();

  void findCommonNodes(Mesh& other);
  void findCommonFaces(StorageSite& faces, StorageSite& otherFaces,
                       const GeomFields& geomFields);

  Mesh* extractBoundaryMesh();
  
protected:
  const int _dimension;
  const int _id;

  StorageSite _cells;
  StorageSite _faces;
  StorageSite _nodes;

  StorageSite _ibFaces;
  mutable StorageSite* _boundaryNodes;
 
  shared_ptr<FaceGroup> _interiorFaceGroup;
  FaceGroupList _faceGroups;
  FaceGroupList _boundaryGroups;
  FaceGroupList _interfaceGroups;
  mutable ConnectivityMap _connectivityMap;
  shared_ptr<Array<VecD3> > _coordinates;
  mutable shared_ptr<Array<int> > _boundaryNodeGlobalToLocalPtr;

  mutable shared_ptr<Array<int> > _ibFaceList;

  shared_ptr< Array<int> > _cellColor;
  shared_ptr< Array<int> > _cellColorOther;
  int  _numOfAssembleMesh;
  bool _isAssembleMesh;

  GhostCellSiteMap   _ghostCellSiteScatterMap;
  GhostCellSiteMap   _ghostCellSiteGatherMap;

 
  //mutable Array<int> *_cellTypes;
  //mutable Array<int> *_cellTypeCount;
  mutable shared_ptr<CRConnectivity> _cellCells2;
  mutable shared_ptr<CRConnectivity> _faceCells2;
};

typedef vector<Mesh*> MeshList;

#endif
