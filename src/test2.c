// 包含头文件
#include "stdefine.h"
#include "ffencoder.h"

int main(void)
{
    void *encoder = NULL;
    BYTE  abuf[1600 * 4     ];
    BYTE  vbuf[256 * 240 * 4];
    void *adata   [8] = { abuf };
    void *vdata   [8] = { vbuf };
    int   linesize[8] = { 256 * 4 };
    int   i, j;

    encoder = ffencoder_init(NULL);
    
    for (i=0; i<500; i++)
    {
        for (j=0; j<1600*2 ; j++) ((short*)abuf)[j] = rand();
        for (j=0; j<256*240; j++)
        {
            vbuf[j*4 + 0] = rand();
            vbuf[j*4 + 1] = rand();
            vbuf[j*4 + 2] = rand();
            vbuf[j*4 + 3] = 0;
        }
        ffencoder_audio(encoder, adata, 1600    );
        ffencoder_video(encoder, vdata, linesize);
    }

    ffencoder_free(encoder);
    return 0;
}
