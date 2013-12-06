#include <tesseract/baseapi.h>
#include <leptonica/allheaders.h>

extern "C" {
	char * decode1(void * buffer, int size);
	void init_captcha();
}

tesseract::TessBaseAPI *api;

void init_captcha() {
	api = new tesseract::TessBaseAPI();
	// Initialize tesseract-ocr with English, without specifying tessdata path
	if (api->Init(NULL, "eng")) {
		fprintf(stderr, "Could not initialize tesseract!\n");
		exit(1);
	}
}

char * decode1(void * buffer, int size) {
	Pix *image = pixReadMem((unsigned char*) buffer, size);
	Pix *gray = pixConvertRGBToLuminance(image);
	Pix *bwimage = pixThresholdToBinary(gray, 128);

	api->SetImage(bwimage);
	// Get OCR result
	char * outText = api->GetUTF8Text();

	if (!outText) outText = "";

	char * rr = (char*)malloc(strlen(outText)+1);
	memcpy(rr,outText,strlen(outText)+1);

	// Now trim every non-numeric char
	for (int i = 0; i < strlen(rr); i++)
		if (!(rr[i] <= '9' && rr[i] >= '0'))
			for (int j = i--; j < strlen(rr); j++)
				rr[j] = rr[j+1];

	// Destroy used object and release memory
	//api->End();
	delete [] outText;
	pixDestroy(&image);
	pixDestroy(&bwimage);
	pixDestroy(&gray);

	return rr;
}
