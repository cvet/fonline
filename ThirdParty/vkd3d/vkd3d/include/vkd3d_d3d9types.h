/*
 * Copyright 2002-2003 Jason Edmeades
 * Copyright 2002-2003 Raphael Junqueira
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#ifndef __VKD3D_D3D9TYPES_H
#define __VKD3D_D3D9TYPES_H
#ifndef _d3d9TYPES_H_

#ifndef MAKEFOURCC
#define MAKEFOURCC(ch0, ch1, ch2, ch3)  \
    ((DWORD)(BYTE)(ch0) | ((DWORD)(BYTE)(ch1) << 8) |  \
    ((DWORD)(BYTE)(ch2) << 16) | ((DWORD)(BYTE)(ch3) << 24 ))
#endif

#define D3DSI_INSTLENGTH_SHIFT  24

#define D3DSP_DCL_USAGE_SHIFT       0
#define D3DSP_DCL_USAGEINDEX_SHIFT  16
#define D3DSP_DSTMOD_SHIFT          20

#define D3DSP_SRCMOD_SHIFT      24

#define D3DSP_REGTYPE_SHIFT     28
#define D3DSP_REGTYPE_SHIFT2    8
#define D3DSP_REGTYPE_MASK      (0x7 << D3DSP_REGTYPE_SHIFT)
#define D3DSP_REGTYPE_MASK2     0x00001800

#define D3DSP_WRITEMASK_0       0x00010000
#define D3DSP_WRITEMASK_1       0x00020000
#define D3DSP_WRITEMASK_2       0x00040000
#define D3DSP_WRITEMASK_3       0x00080000
#define D3DSP_WRITEMASK_ALL     0x000f0000

#define D3DPS_VERSION(major, minor) (0xffff0000 | ((major) << 8) | (minor))
#define D3DVS_VERSION(major, minor) (0xfffe0000 | ((major) << 8) | (minor))

typedef enum _D3DDECLUSAGE
{
    D3DDECLUSAGE_POSITION       = 0x0,
    D3DDECLUSAGE_BLENDWEIGHT    = 0x1,
    D3DDECLUSAGE_BLENDINDICES   = 0x2,
    D3DDECLUSAGE_NORMAL         = 0x3,
    D3DDECLUSAGE_PSIZE          = 0x4,
    D3DDECLUSAGE_TEXCOORD       = 0x5,
    D3DDECLUSAGE_TANGENT        = 0x6,
    D3DDECLUSAGE_BINORMAL       = 0x7,
    D3DDECLUSAGE_TESSFACTOR     = 0x8,
    D3DDECLUSAGE_POSITIONT      = 0x9,
    D3DDECLUSAGE_COLOR          = 0xa,
    D3DDECLUSAGE_FOG            = 0xb,
    D3DDECLUSAGE_DEPTH          = 0xc,
    D3DDECLUSAGE_SAMPLE         = 0xd,
} D3DDECLUSAGE;

typedef enum _D3DSHADER_INSTRUCTION_OPCODE_TYPE
{
    D3DSIO_NOP          = 0x00,
    D3DSIO_MOV          = 0x01,
    D3DSIO_ADD          = 0x02,
    D3DSIO_SUB          = 0x03,
    D3DSIO_MAD          = 0x04,
    D3DSIO_MUL          = 0x05,
    D3DSIO_RCP          = 0x06,
    D3DSIO_RSQ          = 0x07,
    D3DSIO_DP3          = 0x08,
    D3DSIO_DP4          = 0x09,
    D3DSIO_MIN          = 0x0a,
    D3DSIO_MAX          = 0x0b,
    D3DSIO_SLT          = 0x0c,
    D3DSIO_SGE          = 0x0d,
    D3DSIO_EXP          = 0x0e,
    D3DSIO_LOG          = 0x0f,
    D3DSIO_LIT          = 0x10,
    D3DSIO_DST          = 0x11,
    D3DSIO_LRP          = 0x12,
    D3DSIO_FRC          = 0x13,
    D3DSIO_M4x4         = 0x14,
    D3DSIO_M4x3         = 0x15,
    D3DSIO_M3x4         = 0x16,
    D3DSIO_M3x3         = 0x17,
    D3DSIO_M3x2         = 0x18,
    D3DSIO_CALL         = 0x19,
    D3DSIO_CALLNZ       = 0x1a,
    D3DSIO_LOOP         = 0x1b,
    D3DSIO_RET          = 0x1c,
    D3DSIO_ENDLOOP      = 0x1d,
    D3DSIO_LABEL        = 0x1e,
    D3DSIO_DCL          = 0x1f,
    D3DSIO_POW          = 0x20,
    D3DSIO_CRS          = 0x21,
    D3DSIO_SGN          = 0x22,
    D3DSIO_ABS          = 0x23,
    D3DSIO_NRM          = 0x24,
    D3DSIO_SINCOS       = 0x25,
    D3DSIO_REP          = 0x26,
    D3DSIO_ENDREP       = 0x27,
    D3DSIO_IF           = 0x28,
    D3DSIO_IFC          = 0x29,
    D3DSIO_ELSE         = 0x2a,
    D3DSIO_ENDIF        = 0x2b,
    D3DSIO_BREAK        = 0x2c,
    D3DSIO_BREAKC       = 0x2d,
    D3DSIO_MOVA         = 0x2e,
    D3DSIO_DEFB         = 0x2f,
    D3DSIO_DEFI         = 0x30,

    D3DSIO_TEXCOORD     = 0x40,
    D3DSIO_TEXKILL      = 0x41,
    D3DSIO_TEX          = 0x42,
    D3DSIO_TEXBEM       = 0x43,
    D3DSIO_TEXBEML      = 0x44,
    D3DSIO_TEXREG2AR    = 0x45,
    D3DSIO_TEXREG2GB    = 0x46,
    D3DSIO_TEXM3x2PAD   = 0x47,
    D3DSIO_TEXM3x2TEX   = 0x48,
    D3DSIO_TEXM3x3PAD   = 0x49,
    D3DSIO_TEXM3x3TEX   = 0x4a,
    D3DSIO_TEXM3x3DIFF  = 0x4b,
    D3DSIO_TEXM3x3SPEC  = 0x4c,
    D3DSIO_TEXM3x3VSPEC = 0x4d,
    D3DSIO_EXPP         = 0x4e,
    D3DSIO_LOGP         = 0x4f,
    D3DSIO_CND          = 0x50,
    D3DSIO_DEF          = 0x51,
    D3DSIO_TEXREG2RGB   = 0x52,
    D3DSIO_TEXDP3TEX    = 0x53,
    D3DSIO_TEXM3x2DEPTH = 0x54,
    D3DSIO_TEXDP3       = 0x55,
    D3DSIO_TEXM3x3      = 0x56,
    D3DSIO_TEXDEPTH     = 0x57,
    D3DSIO_CMP          = 0x58,
    D3DSIO_BEM          = 0x59,
    D3DSIO_DP2ADD       = 0x5a,
    D3DSIO_DSX          = 0x5b,
    D3DSIO_DSY          = 0x5c,
    D3DSIO_TEXLDD       = 0x5d,
    D3DSIO_SETP         = 0x5e,
    D3DSIO_TEXLDL       = 0x5f,
    D3DSIO_BREAKP       = 0x60,

    D3DSIO_PHASE        = 0xfffd,
    D3DSIO_COMMENT      = 0xfffe,
    D3DSIO_END          = 0xffff,

    D3DSIO_FORCE_DWORD  = 0x7fffffff,
} D3DSHADER_INSTRUCTION_OPCODE_TYPE;

typedef enum _D3DSHADER_PARAM_DSTMOD_TYPE
{
    D3DSPDM_NONE                = 0 << D3DSP_DSTMOD_SHIFT,
    D3DSPDM_SATURATE            = 1 << D3DSP_DSTMOD_SHIFT,
    D3DSPDM_PARTIALPRECISION    = 2 << D3DSP_DSTMOD_SHIFT,
    D3DSPDM_MSAMPCENTROID       = 4 << D3DSP_DSTMOD_SHIFT,

    D3DSPDM_FORCE_DWORD         = 0x7fffffff,
} D3DSHADER_PARAM_DSTMOD_TYPE;

typedef enum _D3DSHADER_PARAM_REGISTER_TYPE
{
    D3DSPR_TEMP         = 0x00,
    D3DSPR_INPUT        = 0x01,
    D3DSPR_CONST        = 0x02,
    D3DSPR_ADDR         = 0x03,
    D3DSPR_TEXTURE      = 0x03,
    D3DSPR_RASTOUT      = 0x04,
    D3DSPR_ATTROUT      = 0x05,
    D3DSPR_TEXCRDOUT    = 0x06,
    D3DSPR_OUTPUT       = 0x06,
    D3DSPR_CONSTINT     = 0x07,
    D3DSPR_COLOROUT     = 0x08,
    D3DSPR_DEPTHOUT     = 0x09,
    D3DSPR_SAMPLER      = 0x0a,
    D3DSPR_CONST2       = 0x0b,
    D3DSPR_CONST3       = 0x0c,
    D3DSPR_CONST4       = 0x0d,
    D3DSPR_CONSTBOOL    = 0x0e,
    D3DSPR_LOOP         = 0x0f,
    D3DSPR_TEMPFLOAT16  = 0x10,
    D3DSPR_MISCTYPE     = 0x11,
    D3DSPR_LABEL        = 0x12,
    D3DSPR_PREDICATE    = 0x13,

    D3DSPR_FORCE_DWORD  = 0x7fffffff,
} D3DSHADER_PARAM_REGISTER_TYPE;

typedef enum _D3DSHADER_PARAM_SRCMOD_TYPE
{
    D3DSPSM_NONE        = 0x0 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_NEG         = 0x1 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_BIAS        = 0x2 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_BIASNEG     = 0x3 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_SIGN        = 0x4 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_SIGNNEG     = 0x5 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_COMP        = 0x6 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_X2          = 0x7 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_X2NEG       = 0x8 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_DZ          = 0x9 << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_DW          = 0xa << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_ABS         = 0xb << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_ABSNEG      = 0xc << D3DSP_SRCMOD_SHIFT,
    D3DSPSM_NOT         = 0xd << D3DSP_SRCMOD_SHIFT,

    D3DSPSM_FORCE_DWORD = 0x7fffffff,
} D3DSHADER_PARAM_SRCMOD_TYPE;

typedef enum _D3DSHADER_MISCTYPE_OFFSETS
{
    D3DSMO_POSITION  = 0x0,
    D3DSMO_FACE      = 0x1,
} D3DSHADER_MISCTYPE_OFFSETS;

typedef enum _D3DVS_RASTOUT_OFFSETS
{
    D3DSRO_POSITION     = 0x0,
    D3DSRO_FOG          = 0x1,
    D3DSRO_POINT_SIZE   = 0x2,

    D3DSRO_FORCE_DWORD  = 0x7fffffff,
} D3DVS_RASTOUT_OFFSETS;

#endif  /* _d3d9TYPES_H_ */
#endif  /* __VKD3D_D3D9TYPES_H */
