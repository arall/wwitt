
#include <brotli/encode.h>
#include <brotli/decode.h>
#include <string>

std::string brotliarchive(std::string in) {
	BrotliEncoderState* s = BrotliEncoderCreateInstance(0, 0, 0);
	BrotliEncoderSetParameter(s, BROTLI_PARAM_QUALITY, 10);
	BrotliEncoderSetParameter(s, BROTLI_PARAM_LGWIN, BROTLI_MAX_WINDOW_BITS);

	const unsigned outup = in.size() * 2 + 2048;
	uint8_t *tptr = (uint8_t*)malloc(outup);
	size_t available_in = in.size();
	size_t available_out = outup;
	const uint8_t *next_in = (uint8_t*)in.data();
	uint8_t *next_out = tptr;
	size_t outsize;
	BrotliEncoderCompressStream(s, BROTLI_OPERATION_FINISH, &available_in, &next_in, &available_out, &next_out, &outsize);

	std::string ret((char*)tptr, outsize);
	BrotliEncoderDestroyInstance(s);
	free(tptr);

	return ret;
}

std::string brotlidearchive(std::string in) {
	BrotliDecoderState* s = BrotliDecoderCreateInstance(0, 0, 0);

	std::string ret(in.size() * 4 + 16*1024, '\0');
	for (unsigned i = 0; i < 5; i++) {
		size_t out_size = ret.size();
		auto res = BrotliDecoderDecompress(in.size(), (uint8_t*)in.c_str(), &out_size, (uint8_t*)ret.c_str());
		if (res == BROTLI_DECODER_RESULT_SUCCESS) {
			BrotliDecoderDestroyInstance(s);
			return std::string(ret.c_str(), ret.size());
		}
		ret = std::string(ret.size() * 2, '\0');
	}
    
	BrotliDecoderDestroyInstance(s);
	return "";
}

