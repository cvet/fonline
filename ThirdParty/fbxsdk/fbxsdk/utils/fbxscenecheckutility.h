/****************************************************************************************
 
   Copyright (C) 2016 Autodesk, Inc.
   All rights reserved.
 
   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.
 
****************************************************************************************/

//! \file fbxscenecheckutility.h
#ifndef _FBXSDK_SCENE_CHECK_UTILITY_H_
#define _FBXSDK_SCENE_CHECK_UTILITY_H_

#include <fbxsdk/fbxsdk_def.h>
#include <fbxsdk/core/base/fbxarray.h>
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/fbxsdk_nsbegin.h>

class FbxScene;
class FbxStatus;

/** \brief This class defines functions to check the received scene graph for issues.
  * remark This is still an experimental class and it is not expected to validate
           every single data object.
  */
class FBXSDK_DLL FbxSceneCheckUtility 
{
public:
	enum ECheckMode {
		eCheckCycles = 0x01,
		eCheckAnimationData = 0x06,
			eCheckAnimationEmptyLayers = 0x02,
			eCheckAnimatioCurveData = 0x04,
		eCheckGeometryData = 0x7FFF8,
			eCheckPolyVertex = 0x00008,
			eCheckLayerElems = 0x0FFF0,
			// leave room for more granularity
			eCheckOtherData = 0xF0000,
				eCheckSkin    = 0x00010000,
				eCheckCluster = 0x00020000,
				eCheckShape   = 0x00040000,
			    eSelectionNode= 0x00080000,
		eCkeckData = 0x7FFFE // includes GeometryData,AnimationData & OtherData
	};

	/** Construct the object
	  * \param pScene  Input scene to check
	  * \param pStatus FbxStatus object to set error codes in (optional)
	  * \param pDetails Details messages of issues found (optional)
	  *
	  * \remark The Details array and its content must be cleared by the caller
	  */
	FbxSceneCheckUtility(const FbxScene* pScene, FbxStatus* pStatus=NULL, FbxArray<FbxString*>* pDetails = NULL);
	~FbxSceneCheckUtility();

	/** Check for issues in the scene
	  * \param pCheckMode Identify what kind of checks to perform
	  * \param pTryToRemoveBadData If set, this class will try to destroy the objects that contains bad data
	  *        as specified by the check mode flags.
	  * \return	\false if any issue is found in the scene
	  * \remark  Depending on the check mode settings, the processing time can increase dramatically.
	  * \remark  If a status and/or details object is provided, the error code is set and, details info is
	            logged.
	  */
	bool Validate(ECheckMode pCheckMode=eCheckCycles, bool pTryToRemoveBadData=false);

/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
	bool Validate(FbxGeometry* pGeom, int pCheckMode = eCheckGeometryData);

	template <typename T> 
	bool ValidateArray(T* pData, int pNbItems, T pMinVal, T pMaxVal, T* pFixVal = nullptr);

	// Check functions return true if valid, false otherwise
	static bool ValidateMappingMode(FbxLayerElement::EMappingMode pMappingMode);
	static bool ValidateReferenceMode(FbxLayerElement::EReferenceMode pReferenceMode);
	static bool ValidateSurfaceMode(FbxGeometry::ESurfaceMode pSurfaceMode);

	// Return 0 if an error is detected or the expected size of the reference array based on the mapping & reference modes
	static int ExpectedNbItems(const FbxGeometryBase* pGeom, FbxLayerElement::EMappingMode pMappingMode);


	/** Validate that the data in the given structure is within acceptable ranges
	  * return \false if any issue is found
	  */
	static bool ValidateObjectData(const FbxNurbs* pNurbs);
	static bool ValidateObjectData(const FbxNurbsCurve* pNurbs);
	static bool ValidateObjectData(const FbxNurbsSurface* pNurbs);

protected:
	bool HaveCycles();
	bool HaveInvalidData(int pCheckMode);

private:
	FbxSceneCheckUtility();
	FbxSceneCheckUtility(const FbxSceneCheckUtility& pOther); 

	bool CheckMappingMode(FbxLayerElement::EMappingMode pMappingMode, const FbxString& pPrefix);
	bool CheckReferenceMode(FbxLayerElement::EReferenceMode pReferenceMode, const FbxString& pPrefix);
	bool CheckSurfaceMode(FbxGeometry::ESurfaceMode pSurfaceMode, const FbxString& pPrefix);

	template<class T> 
	bool CheckSurfaceType(T pSurfaceType, const FbxString& pPrefix, const char* pDir);

	enum {
		eNoRestriction,
		eDirectOnly,
		eIndexOnly,
	};

	template <class T>
	bool ResetLayerElement(FbxLayerElementTemplate<T>* pLET, FbxString& pMsg);

	template <class T> 
	bool CheckLayerElement(FbxLayerElementTemplate<T>* pLET, 
						   int pMaxCount, 
						   const char* pId, 
						   const FbxString& pPrefix,
						   int pRestriction = eNoRestriction);

	bool MeshHaveInvalidData(int pCheckMode, FbxGeometry* pGeom, const FbxString& pName);
	bool NurbsHaveInvalidData(int pCheckMode, FbxGeometry* pGeom, const FbxString& pName);
	bool LineHaveInvalidData(int pCheckMode, FbxGeometry* pGeom, const FbxString& pName);
	bool GeometryHaveInvalidData(int pCheckMode, FbxGeometry* pGeom, const FbxString& pBase);

    bool LayersHaveInvalidData(FbxGeometryBase* pGeom, const FbxString& pBase, int pNbConnectedMaterials=0);
    bool ClusterHaveInvalidData(FbxCluster* pCluster, const FbxString& pBase, int lMaxValue=-1);
    
    bool ShapeHaveInvalidData();
    bool SelectionNodeHaveInvalidData();
	bool GlobalSettingsHaveInvalidData();

	bool AnimationHaveInvalidData(int pCheckMode);
	bool AnimationHaveEmptyLayers();
	bool AnimationHaveInvalidCurveData();

	const FbxScene* mScene;

	FbxStatus*            mStatus;
	FbxArray<FbxString*>* mDetails;
	FbxString             mBuffer;
	bool                  mCanTryToRecover;

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_UTILS_ROOT_NODE_UTILITY_H_ */
