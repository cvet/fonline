/*
 * (FOnline Patch) Module list of the engine build of FreeType: the TrueType driver with the SFNT loader, the
 * automatic hinter and the grey and monochrome rasterizers. It replaces include/freetype/config/ftmodule.h of the
 * upstream tree, which lists every module the full library builds.
 */

FT_USE_MODULE( FT_Module_Class, autofit_module_class )
FT_USE_MODULE( FT_Driver_ClassRec, tt_driver_class )
FT_USE_MODULE( FT_Module_Class, psnames_module_class )
FT_USE_MODULE( FT_Module_Class, sfnt_module_class )
FT_USE_MODULE( FT_Renderer_Class, ft_smooth_renderer_class )
FT_USE_MODULE( FT_Renderer_Class, ft_raster1_renderer_class )

/* EOF */
