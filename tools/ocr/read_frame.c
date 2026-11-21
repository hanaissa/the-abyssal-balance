/* Host runner for the identical production C OCR, not a Python recognizer. */
#include <stdio.h>
#include <stdlib.h>
#include "OCR.h"
int main(int argc,char **argv) {
    if (argc<2 || argc>8) {
        fprintf(stderr,"Usage: OCR_Read frame.rgb565 [rotation [x y width height [light_text]]]\n"); return 2;
    }
    static uint8_t pixels[CAMERA_FRAME_BYTES];
    FILE *file=fopen(argv[1],"rb");
    if (!file) { perror(argv[1]); return 2; }
    size_t count=fread(pixels,1,sizeof pixels,file);
    int extra=fgetc(file); fclose(file);
    if (count!=sizeof pixels || extra!=EOF) { fputs("Expected exact 320x240 RGB565 file\n",stderr); return 2; }
    ocr_init(); OCRConfig settings={.single_line=true};
    if (argc>2) settings.rotation=(uint8_t)atoi(argv[2]);
    if (argc>=7) {
        settings.x=(uint16_t)atoi(argv[3]); settings.y=(uint16_t)atoi(argv[4]);
        settings.width=(uint16_t)atoi(argv[5]); settings.height=(uint16_t)atoi(argv[6]);
    }
    if (argc==8) settings.light_text=atoi(argv[7])!=0;
    if (ocr_configure(&settings)!=APP_OK) return 2;
    CameraFrame frame={pixels,sizeof pixels,320,240}; char text[256];
    AppStatus status=ocr_read_frame(&frame,text,sizeof text);
    OCRDiagnostics d; ocr_get_diagnostics(&d);
    printf("status=%d components=%u glyphs=%u lines=%u rejected=%u min_confidence=%u\n",
           status,d.components,d.glyphs,d.lines,d.rejected,d.minimum_confidence);
    if (status==APP_OK) printf("%s\n",text);
    return status==APP_OK?0:1;
}
