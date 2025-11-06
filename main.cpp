#include <3ds.h>
#include <stdio.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <algorithm>
#include <tremor/ivorbisfile.h>
#include <cstring>

#define BUFFER_SIZE 8192
#define NUM_BUFFERS 4
#define MUSIC_DIR "sdmc:/3ds/Spot3DiSify/music/"

static s16* audioBuffers[NUM_BUFFERS];
static ndspWaveBuf waveBufs[NUM_BUFFERS];

OggVorbis_File vf;
vorbis_info* vi;

bool playOGG(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        printf("Failed to open: %s\n", path);
        return false;
    }

    if (ov_open(file, &vf, NULL, 0) < 0) {
        printf("Not a valid OGG file.\n");
        fclose(file);
        return false;
    }

    vi = ov_info(&vf, -1);
    ndspChnReset(0);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, vi->rate);
    ndspChnSetFormat(0, vi->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        audioBuffers[i] = (s16*)linearAlloc(BUFFER_SIZE);
        memset(&waveBufs[i], 0, sizeof(ndspWaveBuf));
        waveBufs[i].data_pcm16 = audioBuffers[i];
    }

    int currentBuf = 0;
    int current_section;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();
        if (kDown & KEY_RIGHT || kDown & KEY_LEFT || kDown & KEY_START) break;

        long ret = ov_read(&vf, (char*)audioBuffers[currentBuf], BUFFER_SIZE, &current_section);
        if (ret == 0) break;
        if (ret < 0) continue;

        ndspWaveBuf* buf = &waveBufs[currentBuf];
        while (buf->status != NDSP_WBUF_FREE && buf->status != NDSP_WBUF_DONE) {
            svcSleepThread(1000000);
        }

        memset(buf, 0, sizeof(ndspWaveBuf));
        buf->data_pcm16 = audioBuffers[currentBuf];
        buf->nsamples = ret / (vi->channels * sizeof(s16));
        ndspChnWaveBufAdd(0, buf);

        currentBuf = (currentBuf + 1) % NUM_BUFFERS;
    }

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        while (waveBufs[i].status != NDSP_WBUF_DONE) {
            svcSleepThread(1000000);
        }
    }

    ndspChnReset(0);
    ov_clear(&vf);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        linearFree(audioBuffers[i]);
    }

    return true;
}

std::vector<std::string> getOggFiles(const char* directory) {
    std::vector<std::string> files;
    DIR* dir = opendir(directory);
    if (!dir) return files;

    struct dirent* ent;
    while ((ent = readdir(dir)) != NULL) {
        std::string name = ent->d_name;
        if (name.length() > 4 && name.substr(name.length() - 4) == ".ogg") {
            files.push_back(name);
        }
    }
    closedir(dir);
    std::sort(files.begin(), files.end());
    return files;
}

int main() {
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    ndspInit();

    std::vector<std::string> playlist = getOggFiles(MUSIC_DIR);
    if (playlist.empty()) {
        printf("No .ogg files found in:\n%s\n", MUSIC_DIR);
    } else {
        int index = 0;
        while (aptMainLoop()) {
            consoleClear();
            std::string fullPath = std::string(MUSIC_DIR) + playlist[index];
            printf("Now playing:\n%s\n", playlist[index].c_str());

            playOGG(fullPath.c_str());

            hidScanInput();
            u32 kDown = hidKeysDown();
            if (kDown & KEY_START) break;
            if (kDown & KEY_RIGHT) index = (index + 1) % playlist.size();
            else if (kDown & KEY_LEFT) index = (index - 1 + playlist.size()) % playlist.size();
            else index = (index + 1) % playlist.size(); // auto-advance
        }
    }

    ndspExit();
    gfxExit();
    return 0;
}
