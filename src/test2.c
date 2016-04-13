// 包含头文件
#include "stdefine.h"
#include "ffencoder.h"

static void rand_buf(void *buf, int size)
{
    DWORD *ptrdw = (DWORD*)buf;
    while (size) {
        *ptrdw++ = rand();
        size -= 4;
    }
}

int main(void)
{
    void *encoder = NULL;
    BYTE  abuf[1600 * 4     ];
    BYTE  vbuf[256 * 240 * 4];
    void *adata   [8] = { abuf };
    void *vdata   [8] = { vbuf };
    int   linesize[8] = { 256 * 4 };
    int   i;

    encoder = ffencoder_init(NULL);
    
    for (i=0; i<500; i++)
    {
        rand_buf(abuf, sizeof(abuf));
        ffencoder_audio(encoder, adata, 1600    );

        rand_buf(vbuf, sizeof(vbuf));
        ffencoder_video(encoder, vdata, linesize);
    }

    ffencoder_free(encoder);
    return 0;
}
