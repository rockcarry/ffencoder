// 包含头文件
#include <stdlib.h>
#include <stdint.h>
#include "ffencoder.h"

static void rand_buf(void *buf, int size)
{
    uint32_t *ptrdw = (uint32_t*)buf;
    while (size) {
        *ptrdw++ = rand();
        size -= 4;
    }
}

int main(void)
{
    void    *encoder = NULL;
    uint8_t  abuf[1600 * 4     ];
    uint8_t  vbuf[256 * 240 * 4];
    void    *adata   [8] = { abuf };
    void    *vdata   [8] = { vbuf };
    int      linesize[8] = { 256 * 4 };
    int      i;

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
