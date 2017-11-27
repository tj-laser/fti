/**
 *  Copyright (c) 2017 Leonardo A. Bautista-Gomez
 *  All rights reserved
 *
 *  FTI - A multi-level checkpointing library for C/C++/Fortran applications
 *
 *  Revision 1.0 : Fault Tolerance Interface (FTI)
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *  1. Redistributions of source code must retain the above copyright notice, this
 *  list of conditions and the following disclaimer.
 *
 *  2. Redistributions in binary form must reproduce the above copyright notice,
 *  this list of conditions and the following disclaimer in the documentation
 *  and/or other materials provided with the distribution.
 *
 *  3. Neither the name of the copyright holder nor the names of its contributors
 *  may be used to endorse or promote products derived from this software without
 *  specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 *  ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 *  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 *  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 *  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 *  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 *  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *  @file   hdf5.c
 *  @date   November, 2017
 *  @brief  Funtions to support HDF5 checkpointing.
 */

#ifdef ENABLE_HDF5
#include "interface.h"

/*-------------------------------------------------------------------------*/
/**
    @brief      Writes ckpt to using HDF5 file format.
    @param      FTI_Conf        Configuration metadata.
    @param      FTI_Exec        Execution metadata.
    @param      FTI_Topo        Topology metadata.
    @param      FTI_Ckpt        Checkpoint metadata.
    @param      FTI_Data        Dataset metadata.
    @return     integer         FTI_SCES if successful.

**/
/*-------------------------------------------------------------------------*/
int FTI_WriteHDF5(FTIT_configuration* FTI_Conf, FTIT_execution* FTI_Exec,
                    FTIT_topology* FTI_Topo, FTIT_checkpoint* FTI_Ckpt,
                    FTIT_dataset* FTI_Data)
{
   FTI_Print("I/O mode: HDF5.", FTI_DBUG);
   char str[FTI_BUFS], fn[FTI_BUFS];
   int level = FTI_Exec->ckptLvel;
   if (level == 4 && FTI_Ckpt[4].isInline) { //If inline L4 save directly to global directory
       sprintf(fn, "%s/%s", FTI_Conf->gTmpDir, FTI_Exec->meta[0].ckptFile);
   }
   else {
       sprintf(fn, "%s/%s", FTI_Conf->lTmpDir, FTI_Exec->meta[0].ckptFile);
   }

   //Creating new hdf5 file
   hid_t file_id = H5Fcreate(fn, H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);
   if (file_id < 0) {
      sprintf(str, "FTI checkpoint file (%s) could not be opened.", fn);
      FTI_Print(str, FTI_EROR);

      return FTI_NSCS;
   }

   // write data into ckpt file
   int i;
   for (i = 0; i < FTI_Exec->nbVar; i++) {
      hid_t h5Type = FTI_Data[i].type.h5datatype;
      hsize_t count = FTI_Data[i].count;
      if (FTI_Data[i].type.id > 10 && FTI_Data[i].type.structure == NULL) {
          //if used FTI_InitType() save as binary (using chars)
          h5Type = H5Tcopy(H5T_NATIVE_CHAR);
          H5Tset_size(h5Type, FTI_Data[i].size);
          count = 1; //only 1 array of chars
      }
      //rank will be always 1 <= can protect only 1 dimension array
      herr_t res = H5LTmake_dataset(file_id, FTI_Data[i].name, 1, &count, h5Type, FTI_Data[i].ptr);
      if (res < 0) {
         sprintf(str, "Dataset #%d could not be written", FTI_Data[i].id);
         FTI_Print(str, FTI_EROR);
         H5Fclose(file_id);
         return FTI_NSCS;
      }
   }

   // close file
   if (H5Fclose(file_id) < 0) {
      FTI_Print("FTI checkpoint file could not be closed.", FTI_EROR);

      return FTI_NSCS;
   }

   return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      It loads the HDF5 checkpoint data.
    @return     integer         FTI_SCES if successful.

    This function loads the checkpoint data from the checkpoint file and
    it updates checkpoint information.

 **/
/*-------------------------------------------------------------------------*/
int FTI_RecoverHDF5(FTIT_execution* FTI_Exec, FTIT_checkpoint* FTI_Ckpt,
                   FTIT_dataset* FTI_Data)
{
    char str[FTI_BUFS], fn[FTI_BUFS];
    sprintf(fn, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);

    sprintf(str, "Trying to load FTI checkpoint file (%s)...", fn);
    FTI_Print(str, FTI_DBUG);

    hid_t file_id = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0) {
        FTI_Print("Could not open FTI checkpoint file.", FTI_EROR);
        return FTI_NREC;
    }

    int i;
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        hid_t h5Type = FTI_Data[i].type.h5datatype;
        if (FTI_Data[i].type.id > 10 && FTI_Data[i].type.structure == NULL) {
            //if used FTI_InitType() read as binary (using chars)
            h5Type = H5Tcopy(H5T_NATIVE_CHAR);
            H5Tset_size(h5Type, FTI_Data[i].size);
        }

        herr_t res = H5LTread_dataset(file_id, FTI_Data[i].name, h5Type, FTI_Data[i].ptr);
        if (res < 0) {
            FTI_Print("Could not read FTI checkpoint file.", FTI_EROR);
            H5Fclose(file_id);
            return FTI_NREC;
        }
    }
    if (H5Fclose(file_id) < 0) {
        FTI_Print("Could not close FTI checkpoint file.", FTI_EROR);
        return FTI_NREC;
    }
    FTI_Exec->reco = 0;
    return FTI_SCES;
}

/*-------------------------------------------------------------------------*/
/**
    @brief      During the restart, recovers the given variable
    @param      id              Variable to recover
    @return     int             FTI_SCES if successful.

    During a restart process, this function recovers the variable specified
    by the given id.

 **/
/*-------------------------------------------------------------------------*/
int FTI_RecoverVarHDF5(FTIT_execution* FTI_Exec, FTIT_checkpoint* FTI_Ckpt,
                       FTIT_dataset* FTI_Data, int id)
{
    char str[FTI_BUFS], fn[FTI_BUFS];
    sprintf(fn, "%s/%s", FTI_Ckpt[FTI_Exec->ckptLvel].dir, FTI_Exec->meta[FTI_Exec->ckptLvel].ckptFile);

    sprintf(str, "Trying to load FTI checkpoint file (%s)...", fn);
    FTI_Print(str, FTI_DBUG);

    hid_t file_id = H5Fopen(fn, H5F_ACC_RDONLY, H5P_DEFAULT);
    if (file_id < 0) {
        FTI_Print("Could not open FTI checkpoint file.", FTI_EROR);
        return FTI_NREC;
    }

    int i;
    for (i = 0; i < FTI_Exec->nbVar; i++) {
        if (FTI_Data[i].id == id) {
            break;
        }
    }

    hid_t h5Type = FTI_Data[i].type.h5datatype;
    if (FTI_Data[i].type.id > 10 && FTI_Data[i].type.structure == NULL) {
        //if used FTI_InitType() save as binary
        h5Type = H5Tcopy(H5T_NATIVE_CHAR);
        H5Tset_size(h5Type, FTI_Data[i].size);
    }
    herr_t res = H5LTread_dataset(file_id, FTI_Data[i].name, h5Type, FTI_Data[i].ptr);
    if (res < 0) {
        FTI_Print("Could not read FTI checkpoint file.", FTI_EROR);
        H5Fclose(file_id);
        return FTI_NREC;
    }

    if (H5Fclose(file_id) < 0) {
        FTI_Print("Could not close FTI checkpoint file.", FTI_EROR);
        return FTI_NREC;
    }
    return FTI_SCES;
}

#endif
