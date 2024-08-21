/*
 * Copyright 2024 wtcat
 */
#ifndef lz4_namespace_h_
#define lz4_namespace_h_

#define LZ4_renormDictT \
	lz4_renormDictT

#define LZ4_decompress_safe \
	lz4_decompress_safe

#define LZ4_decompress_safe_withSmallPrefix \
	lz4_decompress_safe_withSmallPrefix
	
#define LZ4_decompress_fast_extDict \
	lz4_decompress_fast_extDict
	
#define LZ4_versionNumber \
	lz4_versionNumber
	
#define LZ4_versionString \
	lz4_versionString
	
#define LZ4_compressBound \
	lz4_compressBound
	
#define LZ4_sizeofState \
	lz4_sizeofState
	
#define LZ4_compress_fast_extState_fastReset \
	lz4_compress_fast_extState_fastReset
	
#define LZ4_initStream \
	lz4_initStream
	
#define LZ4_compress_fast_extState \
	lz4_compress_fast_extState
	
#define LZ4_compress_fast \
	lz4_compress_fast
	
#define LZ4_compress_default \
	lz4_compress_default
	
#define LZ4_compress_destSize \
	lz4_compress_destSize
	
#define LZ4_createStream \
	lz4_createStream
	
#define LZ4_resetStream \
	lz4_resetStream
	
#define LZ4_resetStream_fast \
	lz4_resetStream_fast
	
#define LZ4_freeStream \
	lz4_freeStream
	
#define LZ4_loadDict \
	lz4_loadDict
	
#define LZ4_attach_dictionary \
	lz4_attach_dictionary
	
#define LZ4_compress_fast_continue \
	lz4_compress_fast_continue
	
#define LZ4_compress_forceExtDict \
	lz4_compress_forceExtDict
	
#define LZ4_saveDict \
	lz4_saveDict
	
#define LZ4_decompress_safe_partial \
	lz4_decompress_safe_partial
	
#define LZ4_decompress_fast \
	lz4_decompress_fast
	
#define LZ4_decompress_safe_withPrefix64k \
	lz4_decompress_safe_withPrefix64k
	
#define LZ4_decompress_fast_withPrefix64k \
	lz4_decompress_fast_withPrefix64k
	
#define LZ4_decompress_safe_forceExtDict \
	lz4_decompress_safe_forceExtDict
	
#define LZ4_createStreamDecode \
	lz4_createStreamDecode
	
#define LZ4_freeStreamDecode \
	lz4_freeStreamDecode
	
#define LZ4_setStreamDecode \
	lz4_setStreamDecode
	
#define LZ4_decoderRingBufferSize \
	lz4_decoderRingBufferSize
	
#define LZ4_decompress_safe_continue \
	lz4_decompress_safe_continue
	
#define LZ4_decompress_fast_continue \
	lz4_decompress_fast_continue
	
#define LZ4_decompress_safe_usingDict \
	lz4_decompress_safe_usingDict
	
#define LZ4_decompress_fast_usingDict \
	lz4_decompress_fast_usingDict
	
#define LZ4_compress_limitedOutput \
	lz4_compress_limitedOutput
	
#define LZ4_compress \
	lz4_compress
	
#define LZ4_compress_limitedOutput_withState \
	lz4_compress_limitedOutput_withState
	
#define LZ4_compress_withState \
	lz4_compress_withState
	
#define LZ4_compress_limitedOutput_continue \
	lz4_compress_limitedOutput_continue
	
#define LZ4_compress_continue \
	lz4_compress_continue
	
#define LZ4_uncompress \
	lz4_uncompress
	
#define LZ4_uncompress_unknownOutputSize \
	lz4_uncompress_unknownOutputSize
	
#define LZ4_sizeofStreamState \
	lz4_sizeofStreamState
	
#define LZ4_resetStreamState \
	lz4_resetStreamState
	
#define LZ4_create \
	lz4_create
	
#define LZ4_slideInputBuffer \
	lz4_slideInputBuffer

#define LZ4HC_rotatePattern \
	lz4hc_rotatePattern
	
#define LZ4HC_setExternalDict \
	lz4hc_setExternalDict
	
#define LZ4HC_reverseCountPattern \
	lz4hc_reverseCountPattern
	
#define LZ4HC_countPattern \
	lz4hc_countPattern
	
#define LZ4HC_compress_optimal \
	lz4hc_compress_optimal
	
#define LZ4HC_init_internal \
	lz4hc_init_internal
	
#define LZ4HC_compress_generic_noDictCtx \
	lz4hc_compress_generic_noDictCtx
	
#define LZ4HC_compress_generic \
	lz4hc_compress_generic
	
#define LZ4_sizeofStateHC \
	lz4_sizeofStateHC
	
#define LZ4_createStreamHC \
	lz4_createStreamHC
	
#define LZ4_freeStreamHC \
	lz4_freeStreamHC
	
#define LZ4_initStreamHC \
	lz4_initStreamHC
	
#define LZ4_setCompressionLevel \
	lz4_setCompressionLevel
	
#define LZ4_compress_HC_destSize \
	lz4_compress_HC_destSize
	
#define LZ4_resetStreamHC \
	lz4_resetStreamHC
	
#define LZ4_resetStreamHC_fast \
	lz4_resetStreamHC_fast
	
#define LZ4_compress_HC_extStateHC_fastReset \
	lz4_compress_HC_extStateHC_fastReset
	
#define LZ4_compress_HC_extStateHC \
	lz4_compress_HC_extStateHC
	
#define LZ4_compress_HC \
	lz4_compress_HC
	
#define LZ4_favorDecompressionSpeed \
	lz4_favorDecompressionSpeed
	
#define LZ4_loadDictHC \
	lz4_loadDictHC
	
#define LZ4_compressHC_continue_generic \
	lz4_compressHC_continue_generic
	
#define LZ4_attach_HC_dictionary \
	lz4_attach_HC_dictionary
	
#define LZ4_compress_HC_continue \
	lz4_compress_HC_continue
	
#define LZ4_compress_HC_continue_destSize \
	lz4_compress_HC_continue_destSize
	
#define LZ4_saveDictHC \
	lz4_saveDictHC
	
#define LZ4_compressHC \
	lz4_compressHC
	
#define LZ4_compressHC_limitedOutput \
	lz4_compressHC_limitedOutput
	
#define LZ4_compressHC2 \
	lz4_compressHC2
	
#define LZ4_compressHC2_limitedOutput \
	lz4_compressHC2_limitedOutput
	
#define LZ4_compressHC_withStateHC \
	lz4_compressHC_withStateHC
	
#define LZ4_compressHC_limitedOutput_withStateHC \
	lz4_compressHC_limitedOutput_withStateHC
	
#define LZ4_compressHC2_withStateHC \
	lz4_compressHC2_withStateHC
	
#define LZ4_compressHC2_limitedOutput_withStateHC \
	lz4_compressHC2_limitedOutput_withStateHC
	
#define LZ4_compressHC_continue \
	lz4_compressHC_continue
	
#define LZ4_compressHC_limitedOutput_continue \
	lz4_compressHC_limitedOutput_continue
	
#define LZ4_sizeofStreamStateHC \
	lz4_sizeofStreamStateHC
	
#define LZ4_resetStreamStateHC \
	lz4_resetStreamStateHC
	
#define LZ4_createHC \
	lz4_createHC
	
#define LZ4_freeHC \
	lz4_freeHC
	
#define LZ4_compressHC2_continue \
	lz4_compressHC2_continue
	
#define LZ4_compressHC2_limitedOutput_continue \
	lz4_compressHC2_limitedOutput_continue
	
#define LZ4_slideInputBufferHC \
	lz4_slideInputBufferHC
	
#endif /* lz4_namespace_h_ */
