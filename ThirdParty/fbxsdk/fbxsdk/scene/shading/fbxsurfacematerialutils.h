/****************************************************************************************

   Copyright (C) 2020 Autodesk, Inc.
   All rights reserved.

   Use of this software is subject to the terms of the Autodesk license agreement
   provided at the time of installation or download, or which otherwise accompanies
   this software in either electronic or hard copy form.

****************************************************************************************/

//! \file fbxsurfacematerialutil.h
#ifndef _FBXSDK_SURFACE_MATERIAL_UTILS_H_
#define _FBXSDK_SURFACE_MATERIAL_UTILS_H_


#include <fbxsdk/core/base/fbxarray.h>
#include <fbxsdk/core/base/fbxstring.h>
#include <fbxsdk/core/fbxproperty.h>

#include <fbxsdk/fbxsdk_nsbegin.h>

class FbxScene;
class FbxSurfaceMaterial;
class FbxImplementation;
class FbxImplementationFilter;
class FbxBindingTable;
class FbxDataType;

/** This class defines some utility functions to access FbxSurfaceMaterial properties by name regardless of the actual
  * material definition. These functions are provided for convenience to simplify the manipulation of Materials (especially
  * shader definitions that use the FbxImplementation and the FbxBindingTable which defines a variable number of properties).
  * 
  * \remark For a complete access to the FbxSurfaceMaterial objects, it may still be required to directly access the data using
  *         the relevant objects. 
  *
  * \see The ExportShader and ImportScene samples code

  * Example:
  *     Supposing that the scene contains one Lambert and one Stanadard Surface material, the following code
  *
  *          FbxArray<FbxString*> lNames;
  *          for (int m = 0; m < lScene->GetSrcObjectCount<FbxSurfaceMaterial>(); m++)
  *          {
  *              FbxSurfaceMaterial* lMat = lScene->GetSrcObject<FbxSurfaceMaterial>(m);
  *              FBXSDK_printf("\n\nMATERIAL NAME: %s\n\n", lMat->GetName());
  *              const FbxImplementation* lImpl = FbxSurfaceMaterialUtils::GetImplementation(lMat);
  *
  *              if (lImpl)
  *              {
  *                  FBXSDK_printf("          Language =%s\n", lImpl->Language.Get().Buffer());
  *                  FBXSDK_printf("   LanguageVersion =%s\n", lImpl->LanguageVersion.Get().Buffer());
  *                  FBXSDK_printf("        RenderName =%s\n", lImpl->RenderName.Buffer());
  *                  FBXSDK_printf("         RenderAPI =%s\n", lImpl->RenderAPI.Get().Buffer());
  *                  FBXSDK_printf("  RenderAPIVersion =%s\n\n", lImpl->RenderAPIVersion.Get().Buffer());
  *              }
  *
  *              bool lRes = FbxSurfaceMaterialUtils::GetPropertiesNames(lNames, lMat);
  *              for (int i = 0; lRes && i < lNames.GetCount(); i++)
  *              {
  *                  FbxProperty lP = FbxSurfaceMaterialUtils::GetProperty(lNames[i]->Buffer(), lMat);
  *                  if (lP.IsValid())
  *                  {
  *                      FBXSDK_printf("  %-33s (%-40s) ", lP.GetNameAsCStr(), lP.GetHierarchicalName().Buffer());
  *                      switch (lP.GetPropertyDataType().GetType())
  *                      {
  *                      case EFbxType::eFbxString:      FBXSDK_printf("%s\n", lP.Get<FbxString>().Buffer());                                                                           break;
  *                      case EFbxType::eFbxBool:        FBXSDK_printf("%s\n", ((lP.Get<bool>()) ? "True" : "False"));                                                                  break;
  *                      case EFbxType::eFbxFloat:       FBXSDK_printf("%f\n", lP.Get<float>());                                                                                        break;
  *                      case EFbxType::eFbxDouble:      FBXSDK_printf("%lf\n", lP.Get<double>());                                                                                      break;
  *                      case EFbxType::eFbxDouble2:   { FbxDouble2 v = lP.Get<FbxDouble2>(); FBXSDK_printf("%4.3f %4.3f\n", v[0], v[1]); }                                             break;
  *                      case EFbxType::eFbxDouble3:   { FbxDouble3 c = lP.Get<FbxDouble3>(); FBXSDK_printf("%4.3f %4.3f %4.3f\n", c[0], c[1], c[2]); }                                 break;
  *                      case EFbxType::eFbxDouble4x4: { FbxDouble4x4 c = lP.Get<FbxDouble4x4>(); FBXSDK_printf("%4.3f %4.3f %4.3f %4.3f ...\n", c[0][1], c[0][1], c[0][2], c[0][3]); } break;
  *                      default:
  *                          // more cases can may be added ...
  *                          FBXSDK_printf(" -tbd-\n");
  *                          break;
  *                      };
  *                  }
  *              }
  *              FbxArrayDelete<FbxString*>(lNames);
  *          }
  *
  *      would print this data on the console (values depend on the actual settings on the materials)
  *
  *          MATERIAL NAME: lambert2
  *
  *            ShadingModel                   (ShadingModel                            ) Lambert
  *            MultiLayer                     (MultiLayer                              ) False
  *            EmissiveColor                  (EmissiveColor                           ) 1.000 1.000 0.000
  *            EmissiveFactor                 (EmissiveFactor                          ) 1.000000
  *            AmbientColor                   (AmbientColor                            ) 1.000 1.000 1.000
  *            AmbientFactor                  (AmbientFactor                           ) 1.000000
  *            DiffuseColor                   (DiffuseColor                            ) 1.000 0.000 0.000
  *            DiffuseFactor                  (DiffuseFactor                           ) 0.800000
  *            Bump                           (Bump                                    ) 0.000 0.000 0.000
  *            NormalMap                      (NormalMap                               ) 0.000 0.000 0.000
  *            BumpFactor                     (BumpFactor                              ) 1.000000
  *            TransparentColor               (TransparentColor                        ) 0.368 0.368 0.368
  *            TransparencyFactor             (TransparencyFactor                      ) 1.000000
  *            DisplacementColor              (DisplacementColor                       ) 0.000 0.000 0.000
  *            DisplacementFactor             (DisplacementFactor                      ) 1.000000
  *            VectorDisplacementColor        (VectorDisplacementColor                 ) 0.000 0.000 0.000
  *            VectorDisplacementFactor       (VectorDisplacementFactor                ) 1.000000
  *            Emissive                       (Emissive                                ) 1.000 1.000 0.000
  *            Ambient                        (Ambient                                 ) 1.000 1.000 1.000
  *            Diffuse                        (Diffuse                                 ) 0.800 0.000 0.000
  *            Opacity                        (Opacity                                 ) 0.632258
  *
  *          MATERIAL NAME: standardSurface2
  *
  *                    Language =StandardSSL
  *             LanguageVersion =1.0.1
  *                  RenderName =
  *                   RenderAPI =OSL
  *            RenderAPIVersion =
  *
  *            specular_IOR                     (Maya|specularIOR                        ) 1.500000
  *            emission                         (Maya|emission                           ) 0.000000
  *            thin_film_thickness              (Maya|thinFilmThickness                  ) 0.000000
  *            coat_affect_color                (Maya|coatAffectColor                    ) 0.000000
  *            sheen_color                      (Maya|sheenColor                         ) 1.000 1.000 1.000
  *            sheen                            (Maya|sheen                              ) 0.000000
  *            transmission_depth               (Maya|transmissionDepth                  ) 0.000000
  *            transmission_color               (Maya|transmissionColor                  ) 1.000 1.000 1.000
  *            specular_color                   (Maya|specularColor                      ) 1.000 1.000 1.000
  *            sheen_roughness                  (Maya|sheenRoughness                     ) 0.300000
  *            metalness                        (Maya|metalness                          ) 0.000000
  *            base_color                       (Maya|baseColor                          ) 1.000 1.000 1.000
  *            base                             (Maya|base                               ) 0.800000
  *            coat_normal                      (Maya|coatNormal                         ) 0.000 0.000 0.000
  *            coat_roughness                   (Maya|coatRoughness                      ) 0.100000
  *            transmission_extra_roughness     (Maya|transmissionExtraRoughness         ) 0.000000
  *            specular                         (Maya|specular                           ) 1.000000
  *            subsurface_scale                 (Maya|subsurfaceScale                    ) 1.000000
  *            specular_anisotropy              (Maya|specularAnisotropy                 ) 0.500000
  *            diffuse_roughness                (Maya|diffuseRoughness                   ) 0.000000
  *            transmission_dispersion          (Maya|transmissionDispersion             ) 0.000000
  *            transmission                     (Maya|transmission                       ) 0.000000
  *            emission_color                   (Maya|emissionColor                      ) 1.000 1.000 1.000
  *            subsurface_anisotropy            (Maya|subsurfaceAnisotropy               ) 0.000000
  *            subsurface                       (Maya|subsurface                         ) 0.000000
  *            specular_roughness               (Maya|specularRoughness                  ) 0.000000
  *            coat_clor                        (Maya|coatColor                          ) 1.000 1.000 1.000
  *            transmission_scatter_anisotropy  (Maya|transmissionScatterAnisotropy      ) 0.000000
  *            coat_IOR                         (Maya|coatIOR                            ) 1.500000
  *            specular_rotation                (Maya|specularRotation                   ) 0.000000
  *            coat_affect_roughness            (Maya|coatAffectRoughness                ) 0.000000
  *            coat_rotation                    (Maya|coatRotation                       ) 0.000000
  *            coat                             (Maya|coat                               ) 1.000000
  *            thin_walled                      (Maya|thinWalled                         ) False
  *            opacity                          (Maya|opacity                            ) 1.000 1.000 1.000
  *            thin_film_IOR                    (Maya|thinFilmIOR                        ) 1.500000
  *            normalCamera                     (Maya|normalCamera                       ) 1.000 1.000 1.000
  *            coat_anisotropy                  (Maya|coatAnisotropy                     ) 0.000000
  *            subsurface_radius                (Maya|subsurfaceRadius                   ) 1.000 1.000 1.000
  *            subsurface_color                 (Maya|subsurfaceColor                    ) 1.000 1.000 1.000
  *            transmission_scatter             (Maya|transmissionScatter                ) 0.000 0.000 0.000
  */
class FBXSDK_DLL FbxSurfaceMaterialUtils
{
public:
    /** Fill an array with the name of all defined properties in the FbxSurfaceMaterial.
      *
      * \param pPropertiesNames List of names of all the properties defined on the material.
      * \param pMaterial Material object to query.
      *
      * \param pImplementationId If the \c pMaterial have some implementation definitions, this parameter specifies which one to use.
      * \param pBindingTableId  If the \c pMaterial have some implementation definitions, this parameter specifies which binding table in the
      *        implementation defintion to use.
      *
      * \return \c true if the operation was successfull and \c false otherwise.
      *
      * \remark The strings are allocated with FbxNew and must be deleted by the caller. Also, the \c pPropertiesNames array is not re-initialized
      *         before it is filled and, on error conditions, is left untouched.
      * \remark If an Implementation is defined, the returned names are retrieved from the Destination entries of the Binding table else,
      *         the names are simply the properties names.
      *
      */
    static bool GetPropertiesNames(FbxArray<FbxString*>& pPropertiesNames, const FbxSurfaceMaterial* pMaterial, int pImplementationId=0, int pBindingTableId=0);

    /** Get the named property.
      *
      * \param pPropertyName Name of the property to query (as returned by the GetPropertiesNames() function).
      * \param pMaterial Material object to query.
      *
      * \param pImplementationId If the \c pMaterial have some implementation definitions, this parameter specifies which one to use.
      * \param pBindingTableId  If the \c pMaterial have some implementation definitions, this parameter specifies which binding table in the
      *        implementation defintion to use.
      *
      * \remark The returned property must always be tested for its validity before using it.
      */
    static FbxProperty GetProperty(const char* pPropertyName, const FbxSurfaceMaterial* pMaterial, int pImplementationId = 0, int pBindingTableId = 0);

    /** Get an implementation object from the material.
      * 
      * \param pMaterial Material object to query.
      * \param pImplementationId If the \c pMaterial have some implementation definitions, this parameter specifies which one to use.
      *
      * \return Either the DefaultImplementation, the one specified by the \c pImplementationId index or null if none are present.
      * \remark The DefaultImplementation is returned \b only if no other implementations are connected to the material.
      */
    static const FbxImplementation* GetImplementation(const FbxSurfaceMaterial* pMaterial, int pImplementationId = 0);

    /** Create an empty Shader defintion.
      * This function can be the starting point to define a Shader material.
      *
      * \param pScene Scene containing the newly allocated objects.
      * \param pName Name of the material.
      * \param pShadingLanguage Either one of the FBXSDK_SHADING_LANGUAGE_??? macros or a custom name.
      * \param pShadingLanguageVersion The shader language version.
      * \param pShadingRenderAPI Either one of the FBXSDK_RENDERING_API_??? macros or a custom name.
      * \param pShadingRenderAPIVersion The shader render API version.
      * \param pImplementation Either one of the FBXSDK_IMPLEMENTATION_??? macros or a custom name.
      *
      * \return The allocated FbxSusrfaceMaterial object with empty implementation and binding table already connected.
      * \remark Using a custom name for any of the parameters may cause applications to not understand the material
      *         definition.
      * \remark If an error occurs at any step during the creation, the memory is cleared and the function returns null.
      * \remark Once the FbxSurfaceMaterial is created the client has to manually add the properties to it using the 
      *         AddPropery() function.
     */
    static FbxSurfaceMaterial* CreateShaderMaterial(FbxScene* pScene,
                                                    const char* pName,
                                                    const char* pShadingLanguage, const char* pShadingLanguageVersion,
                                                    const char* pShadingRenderAPI, const char* pShadingRenderAPIVersion,
                                                    const char* pImplementation = nullptr);

    /** Add properties to the binding table of the material.
      *
      * \return The added property if successfully created or an Invalid property if an error occurred.
      * \remark This function expects one single implementation definition on the material and will only add to the "root" table.
      * \remark \c pPropertyName can specify a hierarchical name and the function will automatically create the compound properties,if required
      *         or re-use the existing ones.
      *
      *         for example: pRopertyName = "MyGroup|oneCompound|theProperty" will create a compound property named "MyGroup", another
      *         compound property "oneCompound" child of "MyGroup" and finally the property "theProperty" with \c pDataType type children
      *         of "oneCompound".
      */

    static FbxProperty AddProperty(FbxSurfaceMaterial* pMaterial, const char* pPropertyName, const char* pShaderParam, const FbxDataType& pDataType);

private:
    static const FbxBindingTable* GetTable(const FbxImplementation* pImpl, int pTableId = 0);  
};

#include <fbxsdk/fbxsdk_nsend.h>

#endif /* _FBXSDK_SURFACE_MATERIAL_UTILS_H_ */
