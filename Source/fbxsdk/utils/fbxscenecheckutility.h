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
#include <fbxsdk/fbxsdk_nsbegin.h>

class FbxScene;
class FbxStatus;

/** \brief This class defines functions to check the received scene graph for issues. 
  */
class FBXSDK_DLL FbxSceneCheckUtility 
{
public:
	/** Construct the object
	  * pScene  Input scene to check
	  * pStatus FbxStatus object to set error codes in (optional)
	  */
	FbxSceneCheckUtility(const FbxScene* pScene, FbxStatus* pStatus=NULL);
	~FbxSceneCheckUtility();

	/** Check for issues in the scene
	  * return	\false if any issue is found in the scene
	  * remark  For now, only check for cycles in the graph.
	  * remark  If an FbxStatus object is provided, the error code is set.
	  */
	bool Validate();

/*****************************************************************************************************************************
** WARNING! Anything beyond these lines is for internal use, may not be documented and is subject to change without notice! **
*****************************************************************************************************************************/
#ifndef DOXYGEN_SHOULD_SKIP_THIS
protected:
	bool HaveCycles();
	bool HaveEmptyAnimLayers();

private:
	FbxSceneCheckUtility();
	FbxSceneCheckUtility(const FbxSceneCheckUtility& pOther); 

	const FbxScene* mScene;
	FbxStatus* mStatus;

#endif /* !DOXYGEN_SHOULD_SKIP_THIS *****************************************************************************************/
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_UTILS_ROOT_NODE_UTILITY_H_ */
