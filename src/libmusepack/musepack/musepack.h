/// \file musepack.h
/// Top level include file for libmusepack.

#ifndef _musepack_h_
#define _musepack_h_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#include "musepack/config_types.h"
#include "musepack/decoder.h"
#include "musepack/math.h"
#include "musepack/reader.h"
#include "musepack/streaminfo.h"
    
enum {
    MPC_FRAME_LENGTH = (36 * 32),    /// samples per mpc frame
    MPC_DECODER_BUFFER_LENGTH = 4 * MPC_FRAME_LENGTH /// required buffer size for decoder
};

// error codes
#define ERROR_CODE_OK            0
#define ERROR_CODE_FILE         -1
#define ERROR_CODE_SV7BETA       1
#define ERROR_CODE_CBR           2
#define ERROR_CODE_IS            3
#define ERROR_CODE_BLOCKSIZE     4
#define ERROR_CODE_INVALIDSV     5

/// Initializes a streaminfo structure.
/// \param si streaminfo structure to initialize
void mpc_streaminfo_init(mpc_streaminfo *si);

/// Reads streaminfo header from the mpc stream supplied by r.
/// \param si streaminfo pointer to which info will be written
/// \param r stream reader to supply raw data
/// \return error code
mpc_int32_t mpc_streaminfo_read(mpc_streaminfo *si, mpc_reader *r);

/// Gets length of stream si, in seconds.
/// \return length of stream in seconds
double mpc_streaminfo_get_length(mpc_streaminfo *si);

/// Returns length of stream si, in samples.
/// \return length of stream in samples
mpc_int64_t mpc_streaminfo_get_length_samples(mpc_streaminfo *si);

/// Sets up decoder library.
/// Call this first when preparing to decode an mpc stream.
/// \param r reader that will supply raw data to the decoder
void mpc_decoder_setup(mpc_decoder *d, mpc_reader *r);

/// Initializes mpc decoder with the supplied stream info parameters.
/// Call this next after calling mpc_decoder_setup.
/// \param si streaminfo structure indicating format of source stream
/// \return TRUE if decoder was initalized successfully, FALSE otherwise    
mpc_bool_t mpc_decoder_initialize(mpc_decoder *d, mpc_streaminfo *si);

/// Sets decoder sample scaling factor.  All decoded samples will be multiplied
/// by this factor.
/// \param scale_factor multiplicative scaling factor
void mpc_decoder_scale_output(mpc_decoder *d, double scale_factor);

/// Actually reads data from previously initialized stream.  Call
/// this iteratively to decode the mpc stream.
/// \param buffer destination buffer for decoded samples
/// \param vbr_update_acc \todo document me
/// \param vbr_update_bits \todo document me
/// \return -1 if an error is encountered
/// \return 0 if the stream has been completely decoded successfully and there are no more samples
/// \return > 0 to indicate the number of bytes that were actually read from the stream.
mpc_uint32_t mpc_decoder_decode(
    mpc_decoder *d,
    MPC_SAMPLE_FORMAT *buffer, 
    mpc_uint32_t *vbr_update_acc, 
    mpc_uint32_t *vbr_update_bits);

/// Seeks to the specified sample in the source stream.
mpc_bool_t mpc_decoder_seek_sample(mpc_decoder *d, mpc_int64_t destsample);

/// Seeks to specified position in seconds in the source stream.
mpc_bool_t mpc_decoder_seek_seconds(mpc_decoder *d, double seconds);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // _musepack_h_
