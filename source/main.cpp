#include <3ds.h>
#include <stdio.h>
#include <dirent.h>
#include <string>
#include <vector>
#include <algorithm>
#include <3ds/services/apt.h>
#include <tremor/ivorbisfile.h>
#include <cstring>

#define BUFFER_SIZE 16384
#define NUM_BUFFERS 6
#define MUSIC_DIR "sdmc:/3ds/Spot3DiSify/music/"

static s16* audioBuffers[NUM_BUFFERS];
static ndspWaveBuf waveBufs[NUM_BUFFERS];

OggVorbis_File vf;
vorbis_info* vi;
float volume = 1.0f;

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
    if (!vi || vi->channels <= 0 || vi->channels > 2) {
        printf("Invalid audio format.\n");
        ov_clear(&vf);
        fclose(file);
        return false;
    }

    ndspChnReset(0);
    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnSetInterp(0, NDSP_INTERP_POLYPHASE);
    ndspChnSetRate(0, 44100);
    ndspChnSetFormat(0, vi->channels == 2 ? NDSP_FORMAT_STEREO_PCM16 : NDSP_FORMAT_MONO_PCM16);
    ndspSetMasterVol(volume);

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        audioBuffers[i] = (s16*)linearAlloc(BUFFER_SIZE);
        if (!audioBuffers[i]) {
            printf("Buffer allocation failed.\n");
            for (int j = 0; j < i; ++j) linearFree(audioBuffers[j]);
            ov_clear(&vf);
            fclose(file);
            return false;
        }
        memset(&waveBufs[i], 0, sizeof(ndspWaveBuf));
        waveBufs[i].data_vaddr = audioBuffers[i];
        waveBufs[i].status = NDSP_WBUF_DONE;
    }

    int currentBuf = 0;
    int current_section;

    while (aptMainLoop()) {
        hidScanInput();
        u32 kDown = hidKeysDown();

        if (kDown & KEY_RIGHT || kDown & KEY_LEFT || kDown & KEY_START) break;

        if (kDown & KEY_UP) {
            volume = std::min(volume + 0.1f, 5.0f);
            ndspSetMasterVol(volume);
            printf("Volume: %.0f%%\n", volume * 100);
        }
        if (kDown & KEY_DOWN) {
            volume = std::max(volume - 0.1f, 0.0f);
            ndspSetMasterVol(volume);
            printf("Volume: %.0f%%\n", volume * 100);
        }

        ndspWaveBuf* buf = &waveBufs[currentBuf];
        if (!buf || buf->status != NDSP_WBUF_DONE) {
            svcSleepThread(1000000);
            continue;
        }

        long ret = ov_read(&vf, (char*)audioBuffers[currentBuf], BUFFER_SIZE, &current_section);
        if (ret <= 0) {
            printf("Playback complete or error: %ld\n", ret);
            break;
        }

        buf->nsamples = ret / (vi->channels * sizeof(s16));
        if (buf->nsamples == 0) {
            printf("Zero samples — skipping.\n");
            continue;
        }

        DSP_FlushDataCache(audioBuffers[currentBuf], ret);
        ndspChnWaveBufAdd(0, buf);

        currentBuf = (currentBuf + 1) % NUM_BUFFERS;
    }

    for (int i = 0; i < NUM_BUFFERS; ++i) {
        while (waveBufs[i].status == NDSP_WBUF_QUEUED) {
            svcSleepThread(250000);
        }
    }

    ndspChnReset(0);
    ov_clear(&vf);
    fclose(file);
    for (int i = 0; i < NUM_BUFFERS; ++i) {
        if (audioBuffers[i]) linearFree(audioBuffers[i]);
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
    aptSetSleepAllowed(false);
    std::vector<std::string> playlist = getOggFiles(MUSIC_DIR);
    if (playlist.empty()) {
        printf("No .ogg files found in:\n%s\n", MUSIC_DIR);
    } else {
        int index = 0;
        while (aptMainLoop()) {
            consoleClear();
            std::string fullPath = std::string(MUSIC_DIR) + playlist[index];
            printf("Now playing:\n%s\n", playlist[index].c_str());
            printf("Volume: %.0f%%\n", volume * 100);

            if (!playOGG(fullPath.c_str())) {
                printf("Playback failed. Skipping...\n");
            }

            hidScanInput();
            u32 kDown = hidKeysDown();

            if (kDown & KEY_START) break;
            if (kDown & KEY_RIGHT) {
                index = (index + 1) % playlist.size();
            } else if (kDown & KEY_LEFT) {
                index = (index - 1 + playlist.size()) % playlist.size();
            } else {
                index = (index + 1) % playlist.size(); // auto-advance
            }
        }
    }

    ndspExit();
    gfxExit();
    return 0;
}
