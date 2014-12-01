// 包含头文件
#include "ffencoder.h"
#include "log.h"

int main(void)
{
    void *encoder = NULL;
    BYTE  abuf[1920 * 4     ];
    BYTE  vbuf[320 * 240 * 4];
    void *adata   [8] = { abuf };
    void *vdata   [8] = { vbuf };
    int   linesize[8] = { 320 * 4 };
    int   i, j;

    log_init("test2.log");

    encoder = ffencoder_init(NULL);
    
    for (i=0; i<500; i++)
    {
        for (j=0; j<1920*2 ; j++) ((short*)abuf)[j] = rand();
        for (j=0; j<320*240; j++)
        {
            vbuf[j*4 + 0] = rand();
            vbuf[j*4 + 1] = rand();
            vbuf[j*4 + 2] = rand();
            vbuf[j*4 + 3] = 0;
        }
        ffencoder_audio(encoder, adata, 1920    );
        ffencoder_video(encoder, vdata, linesize);
    }

    ffencoder_free(encoder);

    log_done();
    return 0;
}
