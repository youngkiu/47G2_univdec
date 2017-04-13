/*------------------------------------------------------------------------------
--                                                                            --
--       This software is confidential and proprietary and may be used        --
--        only as expressly authorized by a licensing agreement from          --
--                                                                            --
--                            Hantro Products Oy.                             --
--                                                                            --
--                   (C) COPYRIGHT 2006 HANTRO PRODUCTS OY                    --
--                            ALL RIGHTS RESERVED                             --
--                                                                            --
--                 The entire notice above must be reproduced                 --
--                  on all copies and should not be removed.                  --
--                                                                            --
--------------------------------------------------------------------------------
-
-  Description : ...
-
--------------------------------------------------------------------------------
-
-  Version control information, please leave untouched.
-
-  $RCSfile: vp8hwd_probs.c,v $
-  $Revision: 1.4 $
-  $Date: 2009/12/04 15:16:23 $
-
------------------------------------------------------------------------------*/

#include "../inc/basetype.h"
#include "vp8hwd_probs.h"


static const u8 MvUpdateProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
    { 237, 246, 253, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 250, 250, 252, 254, 254
    },
    { 231, 243, 245, 253, 254, 254, 254, 254, 254,
      254, 254, 254, 254, 254, 251, 251, 254, 254, 254
    }
};

static const u8 Vp8DefaultMvProbs[2][VP8_MV_PROBS_PER_COMPONENT] = {
    { 162, 128, 225, 146, 172, 147, 214, 39, 156,
      128, 129, 132, 75, 145, 178, 206, 239, 254, 254
    },
    { 164, 128, 204, 170, 119, 235, 140, 230, 228,
      128, 130, 130, 74, 148, 180, 203, 236, 254, 254
    }
};

static const u8 Vp7DefaultMvProbs[2][VP7_MV_PROBS_PER_COMPONENT] = {
    { 162, 128, 225, 146, 172, 147, 214, 39, 156,
      247, 210, 135,  68, 138, 220, 239, 246
	},
	{ 164, 128, 204, 170, 119, 235, 140, 230, 228,
      244, 184, 201,  44, 173, 221, 239, 253
    }
};

static const u8 CoeffUpdateProbs[4][8][3][11] =
{
  {
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {176, 246, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {223, 241, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 244, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {234, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 246, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {239, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 253, 255, 254, 255, 255, 255, 255, 255, 255, },
      {250, 255, 254, 255, 254, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {217, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {225, 252, 241, 253, 255, 255, 254, 255, 255, 255, 255, },
      {234, 250, 241, 250, 253, 255, 253, 254, 255, 255, 255, },
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {223, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {238, 253, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 248, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {247, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {186, 251, 250, 255, 255, 255, 255, 255, 255, 255, 255, },
      {234, 251, 244, 254, 255, 255, 255, 255, 255, 255, 255, },
      {251, 251, 243, 253, 254, 255, 254, 255, 255, 255, 255, },
    },
    {
      {255, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {236, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {251, 253, 253, 254, 254, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 254, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
  {
    {
      {248, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 254, 252, 254, 255, 255, 255, 255, 255, 255, 255, },
      {248, 254, 249, 253, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {246, 253, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 254, 251, 254, 254, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 254, 252, 255, 255, 255, 255, 255, 255, 255, 255, },
      {248, 254, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 255, 254, 254, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {245, 251, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {253, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 251, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {252, 253, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 254, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 252, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {249, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 254, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 253, 255, 255, 255, 255, 255, 255, 255, 255, },
      {250, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
    {
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {254, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
      {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, },
    },
  },
};


static const u8 DefaultCoeffProbs [4][8][3][11] =
{
{
{
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 253, 136, 254, 255, 228, 219, 128, 128, 128, 128, 128},
{ 189, 129, 242, 255, 227, 213, 255, 219, 128, 128, 128},
{ 106, 126, 227, 252, 214, 209, 255, 255, 128, 128, 128}
},
{
{ 1, 98, 248, 255, 236, 226, 255, 255, 128, 128, 128},
{ 181, 133, 238, 254, 221, 234, 255, 154, 128, 128, 128},
{ 78, 134, 202, 247, 198, 180, 255, 219, 128, 128, 128}
},
{
{ 1, 185, 249, 255, 243, 255, 128, 128, 128, 128, 128},
{ 184, 150, 247, 255, 236, 224, 128, 128, 128, 128, 128},
{ 77, 110, 216, 255, 236, 230, 128, 128, 128, 128, 128}
},
{
{ 1, 101, 251, 255, 241, 255, 128, 128, 128, 128, 128},
{ 170, 139, 241, 252, 236, 209, 255, 255, 128, 128, 128},
{ 37, 116, 196, 243, 228, 255, 255, 255, 128, 128, 128}
},
{
{ 1, 204, 254, 255, 245, 255, 128, 128, 128, 128, 128},
{ 207, 160, 250, 255, 238, 128, 128, 128, 128, 128, 128},
{ 102, 103, 231, 255, 211, 171, 128, 128, 128, 128, 128}
},
{
{ 1, 152, 252, 255, 240, 255, 128, 128, 128, 128, 128},
{ 177, 135, 243, 255, 234, 225, 128, 128, 128, 128, 128},
{ 80, 129, 211, 255, 194, 224, 128, 128, 128, 128, 128}
},
{
{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 246, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 255, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
}
},
{
{
{ 198, 35, 237, 223, 193, 187, 162, 160, 145, 155, 62},
{ 131, 45, 198, 221, 172, 176, 220, 157, 252, 221, 1},
{ 68, 47, 146, 208, 149, 167, 221, 162, 255, 223, 128}
},
{
{ 1, 149, 241, 255, 221, 224, 255, 255, 128, 128, 128},
{ 184, 141, 234, 253, 222, 220, 255, 199, 128, 128, 128},
{ 81, 99, 181, 242, 176, 190, 249, 202, 255, 255, 128}
},
{
{ 1, 129, 232, 253, 214, 197, 242, 196, 255, 255, 128},
{ 99, 121, 210, 250, 201, 198, 255, 202, 128, 128, 128},
{ 23, 91, 163, 242, 170, 187, 247, 210, 255, 255, 128}
},
{
{ 1, 200, 246, 255, 234, 255, 128, 128, 128, 128, 128},
{ 109, 178, 241, 255, 231, 245, 255, 255, 128, 128, 128},
{ 44, 130, 201, 253, 205, 192, 255, 255, 128, 128, 128}
},
{
{ 1, 132, 239, 251, 219, 209, 255, 165, 128, 128, 128},
{ 94, 136, 225, 251, 218, 190, 255, 255, 128, 128, 128},
{ 22, 100, 174, 245, 186, 161, 255, 199, 128, 128, 128}
},
{
{ 1, 182, 249, 255, 232, 235, 128, 128, 128, 128, 128},
{ 124, 143, 241, 255, 227, 234, 128, 128, 128, 128, 128},
{ 35, 77, 181, 251, 193, 211, 255, 205, 128, 128, 128}
},
{
{ 1, 157, 247, 255, 236, 231, 255, 255, 128, 128, 128},
{ 121, 141, 235, 255, 225, 227, 255, 255, 128, 128, 128},
{ 45, 99, 188, 251, 195, 217, 255, 224, 128, 128, 128}
},
{
{ 1, 1, 251, 255, 213, 255, 128, 128, 128, 128, 128},
{ 203, 1, 248, 255, 255, 128, 128, 128, 128, 128, 128},
{ 137, 1, 177, 255, 224, 255, 128, 128, 128, 128, 128}
}
},
{
{
{ 253, 9, 248, 251, 207, 208, 255, 192, 128, 128, 128},
{ 175, 13, 224, 243, 193, 185, 249, 198, 255, 255, 128},
{ 73, 17, 171, 221, 161, 179, 236, 167, 255, 234, 128}
},
{
{ 1, 95, 247, 253, 212, 183, 255, 255, 128, 128, 128},
{ 239, 90, 244, 250, 211, 209, 255, 255, 128, 128, 128},
{ 155, 77, 195, 248, 188, 195, 255, 255, 128, 128, 128}
},
{
{ 1, 24, 239, 251, 218, 219, 255, 205, 128, 128, 128},
{ 201, 51, 219, 255, 196, 186, 128, 128, 128, 128, 128},
{ 69, 46, 190, 239, 201, 218, 255, 228, 128, 128, 128}
},
{
{ 1, 191, 251, 255, 255, 128, 128, 128, 128, 128, 128},
{ 223, 165, 249, 255, 213, 255, 128, 128, 128, 128, 128},
{ 141, 124, 248, 255, 255, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 16, 248, 255, 255, 128, 128, 128, 128, 128, 128},
{ 190, 36, 230, 255, 236, 255, 128, 128, 128, 128, 128},
{ 149, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 226, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 247, 192, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 240, 128, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 1, 134, 252, 255, 255, 128, 128, 128, 128, 128, 128},
{ 213, 62, 250, 255, 255, 128, 128, 128, 128, 128, 128},
{ 55, 93, 255, 128, 128, 128, 128, 128, 128, 128, 128}
},
{
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128},
{ 128, 128, 128, 128, 128, 128, 128, 128, 128, 128, 128}
}
},
{
{
{ 202, 24, 213, 235, 186, 191, 220, 160, 240, 175, 255},
{ 126, 38, 182, 232, 169, 184, 228, 174, 255, 187, 128},
{ 61, 46, 138, 219, 151, 178, 240, 170, 255, 216, 128}
},
{
{ 1, 112, 230, 250, 199, 191, 247, 159, 255, 255, 128},
{ 166, 109, 228, 252, 211, 215, 255, 174, 128, 128, 128},
    { 39, 77, 162, 232, 172, 180, 245, 178, 255, 255, 128}
},
{
{ 1, 52, 220, 246, 198, 199, 249, 220, 255, 255, 128},
{ 124, 74, 191, 243, 183, 193, 250, 221, 255, 255, 128},
{ 24, 71, 130, 219, 154, 170, 243, 182, 255, 255, 128}
},
{
{ 1, 182, 225, 249, 219, 240, 255, 224, 128, 128, 128},
{ 149, 150, 226, 252, 216, 205, 255, 171, 128, 128, 128},
{ 28, 108, 170, 242, 183, 194, 254, 223, 255, 255, 128}
},
{
{ 1, 81, 230, 252, 204, 203, 255, 192, 128, 128, 128},
{ 123, 102, 209, 247, 188, 196, 255, 233, 128, 128, 128},
{ 20, 95, 153, 243, 164, 173, 255, 203, 128, 128, 128}
},
{
{ 1, 222, 248, 255, 216, 213, 128, 128, 128, 128, 128},
{ 168, 175, 246, 252, 235, 205, 255, 255, 128, 128, 128},
{ 47, 116, 215, 255, 211, 212, 255, 255, 128, 128, 128}
},
{
{ 1, 121, 236, 253, 212, 214, 255, 255, 128, 128, 128},
{ 141, 84, 213, 252, 201, 202, 255, 219, 128, 128, 128},
{ 42, 80, 160, 240, 162, 185, 255, 205, 128, 128, 128}
},
{
{ 1, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 244, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128},
{ 238, 1, 255, 128, 128, 128, 128, 128, 128, 128, 128}
}
}
};


/*------------------------------------------------------------------------------
    Function name   : vp8hwdResetProbs
    Description     : Reset probabilities to default values
    Return type     :
    Argument        :
------------------------------------------------------------------------------*/
void vp8hwdResetProbs( vp8Decoder_t* dec )
{
    u32 i, j, k, l;

    /* Intra-prediction modes */
    dec->entropy.probLuma16x16PredMode[0] = 112;
    dec->entropy.probLuma16x16PredMode[1] = 86;
    dec->entropy.probLuma16x16PredMode[2] = 140;
    dec->entropy.probLuma16x16PredMode[3] = 37;
    dec->entropy.probChromaPredMode[0] = 162;
    dec->entropy.probChromaPredMode[1] = 101;
    dec->entropy.probChromaPredMode[2] = 204;

    /* MV context */
    k = 0;
    if(dec->decMode == VP8HWD_VP8)
    {
        for( i = 0 ; i < 2 ; ++i )
            for( j = 0 ; j < VP8_MV_PROBS_PER_COMPONENT ; ++j, ++k )
                dec->entropy.probMvContext[i][j] = Vp8DefaultMvProbs[i][j];
    }
    else
    {
        for( i = 0 ; i < 2 ; ++i )
            for( j = 0 ; j < VP7_MV_PROBS_PER_COMPONENT ; ++j, ++k )
                dec->entropy.probMvContext[i][j] = Vp7DefaultMvProbs[i][j];
    }

    /* Coefficients */
    for( i = 0 ; i < 4 ; ++i )
        for( j = 0 ; j < 8 ; ++j )
            for( k = 0 ; k < 3 ; ++k )
                for( l = 0 ; l < 11 ; ++l )
                    dec->entropy.probCoeffs[i][j][k][l] =
                        DefaultCoeffProbs[i][j][k][l];
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdDecodeMvUpdate
    Description     : Decode mv probability update from stream
    Return type     : OK/NOK
    Argument        :
------------------------------------------------------------------------------*/
u32  vp8hwdDecodeMvUpdate( vpBoolCoder_t *bc, vp8Decoder_t *dec )
{
    u32 i, j;
    u32 tmp;
    u32 mvProbs;

    if(dec->decMode == VP8HWD_VP8)  mvProbs = VP8_MV_PROBS_PER_COMPONENT;
    else                            mvProbs = VP7_MV_PROBS_PER_COMPONENT;

    for( i = 0 ; i < 2 ; ++i )
    {
        for( j = 0 ; j < mvProbs ; ++j )
        {
            tmp = vp8hwdDecodeBool( bc, MvUpdateProbs[i][j] );
            CHECK_END_OF_STREAM(tmp);
            if( tmp == 1 )
            {
                tmp = vp8hwdReadBits( bc, 7 );
                CHECK_END_OF_STREAM(tmp);
                if( tmp )   tmp = tmp << 1;
                else        tmp = 1;
                dec->entropy.probMvContext[i][j] = tmp;
            }
        }
    }
    return (HANTRO_OK);
}

/*------------------------------------------------------------------------------
    Function name   : vp8hwdDecodeCoeffUpdate
    Description     : Decode coeff probability update from stream
    Return type     : OK/NOK
    Argument        :
------------------------------------------------------------------------------*/
u32  vp8hwdDecodeCoeffUpdate( vpBoolCoder_t *bc, vp8Decoder_t *dec )
{
    u32 i, j, k, l;
    u32 tmp;

    for( i = 0; i < 4; i++ )
    {
        for ( j = 0; j < 8; j++ )
        {
            for ( k = 0; k < 3; k++ )
            {
                for ( l = 0; l < 11; l++ )
                {
                    tmp = vp8hwdDecodeBool( bc, CoeffUpdateProbs[i][j][k][l] );
                    CHECK_END_OF_STREAM(tmp);
                    if ( tmp )
                    {
                        tmp = vp8hwdReadBits( bc, 8 );
                        CHECK_END_OF_STREAM(tmp);
                        dec->entropy.probCoeffs[i][j][k][l] = tmp;
                    }
                }
            }
        }
    }
    return (HANTRO_OK);
}

