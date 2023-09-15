/**
 * @file hexcreator.c
 * @author Pierre Ravenel (pierre.ravenel@univ-grenoble-alpes.fr)
 * @brief
 * @version 0.1
 * @date 2023-02-14
 *
 * @copyright Copyright (c) 2023
 *
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEBUG(...) fprintf(stderr, __VA_ARGS__)

const int64_t hamming_arr[8] = {0x5555555555555555LLU, 0x55555555aaaaaaaaLLU,
                                0x5555aaaa5555aaaaLLU, 0x55aa55aa55aa55aaLLU,
                                0xda5a5a5a5a5a5a5aLLU, 0xe666666666666666LLU,
                                0x6aaaaaaaaaaaaaa9LLU, 0xa996966996696995LLU};
const int ecc_bit_size = 8;
const int i_bit_size = 64;

unsigned int ecc(int64_t input_word) {
    int8_t i_result, ecc_bit;
    int64_t hamming;
    int tmp_ecc_value;
    i_result = 0;
    for (ecc_bit = 0; ecc_bit < ecc_bit_size; ecc_bit++) {
        int bit = 0;
        hamming = hamming_arr[ecc_bit];
        tmp_ecc_value = 0;
        for (bit = 0; bit < i_bit_size; bit++)
            tmp_ecc_value += ((input_word >> bit) & 0x1) &
                             ((hamming >> (i_bit_size - 1 - bit)) & 0x1);
        i_result |= ((tmp_ecc_value & 1) << ecc_bit);
    }
    return i_result & 0xff;
}

char *printBytes(uint64_t bytes) {
    static char buff[255];
    if (bytes >> 30) {
        sprintf(buff, "%.3fGB", bytes / 1024.f / 1024.f / 1024.f);
    } else if (bytes >> 20) {
        sprintf(buff, "%.3fMB", bytes / 1024.f / 1024.f);
    } else if (bytes >> 10) {
        sprintf(buff, "%.3fkB", bytes / 1024.f);
    } else {
        sprintf(buff, "%.3fB", (double)bytes);
    }
    return buff;
}

uint64_t *parse_tensor(char *tensor_raw, uint64_t *tensor_size,
                       uint64_t *tensor_norm) {
    *tensor_size = 1;
    for (char *c = tensor_raw; *c != '\0'; c++) {
        *tensor_size += *c == '-';
    }
    uint64_t *tensor = malloc(*tensor_size * sizeof(uint64_t));
    assert(tensor);

    *tensor_norm = 1;
    char *token;
    char *rest = tensor_raw;
    size_t i = 0;
    for (; (token = strtok_r(rest, "-", &rest)); i++) {
        assert(i < *tensor_size);
        tensor[i] = atoi(token);
        *tensor_norm *= tensor[i];
    }
    assert(i == *tensor_size);
    return tensor;
}

void debug_tensor(uint64_t *tensor, uint64_t tensor_size) {
    DEBUG("[ ");
    for (size_t i = 0; i < tensor_size; i++) {
        DEBUG("%ld ", tensor[i]);
    }
    DEBUG("]");
}

#define LOG2(X) \
    ((unsigned)(8 * sizeof(unsigned long long) - __builtin_clzll((X)) - 1))

/**
 * @brief Compute unique index for position in tensor
 *
 * @param tensor_id
 * @param tensor
 * @param tensor_size
 * @return uint64_t
 */
uint64_t serialise_tensor_id(uint64_t *tensor_id, uint64_t *tensor,
                             uint64_t tensor_size) {
    uint64_t res = 0;
    uint64_t mult_acc = 1;
    for (size_t i = 0; i < tensor_size; i++) {
        res += tensor_id[i] * mult_acc;
        mult_acc *= tensor[i];
    }
    return res;
}

/**
 * @brief unserialise tensor id
 *
 * @param id
 * @param tensor
 * @param tensor_size
 * @param tensor_id
 */
uint64_t *unserialise_tensor_id(uint64_t id, uint64_t *tensor,
                                uint64_t tensor_size, uint64_t *tensor_id) {
    uint64_t tensor_prod = 1;
    for (size_t i = 0; i < tensor_size; i++) {
        tensor_id[i] = id / tensor_prod % tensor[i];
        tensor_prod *= tensor[i];
    }
    return tensor_id;
}
/**
 *
 * @brief Compute cut idx from line addr
 *
 * @param lineaddr
 * @param tensor
 * @param tensor_mode
 * @param tensor_size
 * @param height
 * @return uint64_t*
 */
uint64_t line_to_cut_id(uint64_t lineaddr, uint64_t *tensor, char *tensor_mode,
                        uint64_t tensor_size, uint64_t height,
                        uint64_t *tensor_id) {
    uint64_t mem_size_line = height;
    for (size_t i = 0; i < tensor_size; i++) {
        mem_size_line *= tensor[i];
    }

    assert(__builtin_popcount(mem_size_line) == 1);  // Power of 2
    uint8_t addr_size = LOG2(mem_size_line);

    for (int i = tensor_size - 1; i >= 0; i--) {  // Outer to inner
        // DEBUG("addr_size = %d\n", addr_size);

        assert(__builtin_popcount(tensor[i]) == 1);  // Power of 2
        uint8_t shift = LOG2(tensor[i]);
        uint64_t mask = ((1 << shift) - 1);

        // DEBUG("shilft = %d, mask=%lx\n", shift, mask);

        switch (tensor_mode[i]) {
            case '@': {  // id is @ MSB
                tensor_id[i] = (lineaddr >> (addr_size - shift)) & mask;
                break;
            }
            case 'D': {  // id is @ LSB
                tensor_id[i] = lineaddr & mask;
                lineaddr >>= shift;
                break;
            }
            default: {
                DEBUG("ERROR !");
                exit(2);
            }
        }
        addr_size -= shift;
    }
    assert(addr_size == LOG2(height));  // Residual part
    return serialise_tensor_id(tensor_id, tensor, tensor_size);
}

int main(int argc, char *argv[]) {
    if (argc != 9) {
        DEBUG(
            "usage: %s <binpath=FILE> <respath=FOLDER> <tensor=X-Y-Z-...> "
            "<tensormode=STR> <width=UINT> <height=UINT> <template=STR> "
            "<ecc=[0|1]>\n",
            argv[0]);
        return 1;
    }

    char *binpath = argv[1];
    char *respath = argv[2];
    uint64_t tensor_size, tensor_norm;
    uint64_t *tensor = parse_tensor(argv[3], &tensor_size, &tensor_norm);
    char *tensor_mode = argv[4];
    uint64_t width = atoi(argv[5]);
    uint64_t height = atoi(argv[6]);
    char *template = argv[7];
    uint8_t enable_ecc = atoi(argv[8]);

    uint64_t cut_size = width * height;
    uint64_t mem_size = cut_size * tensor_norm;
    uint64_t num_line_total = height * tensor_norm;

    DEBUG("         binpath : %s\n", binpath);
    DEBUG("         respath : %s\n", respath);
    DEBUG("             ecc : %s\n", enable_ecc ? "TRUE" : "FALSE");
    DEBUG("          tensor : (#%ld) ", tensor_size);
    debug_tensor(tensor, tensor_size);
    DEBUG("\n");
    DEBUG("     tensor mode : (#%ld) %s\n", strlen(tensor_mode), tensor_mode);
    DEBUG("           width : %ld bytes\n", width);
    DEBUG("          height : %ld\n", height);
    DEBUG("         num cut : %ld\n", tensor_norm);
    DEBUG("        cut_size : 0x%lx = %s\n", cut_size, printBytes(cut_size));
    DEBUG("        mem size : 0x%lx = %s\n", mem_size, printBytes(mem_size));
    DEBUG("        template : %s\n", template);

    // Open bin file
    FILE *fp_binfile = fopen(binpath, "rb");
    if (!fp_binfile) {
        fprintf(stderr, "Error: Unable to open %s\n", binpath);
        exit(1);
    }
    fseek(fp_binfile, 0L, SEEK_END);
    uint64_t bin_size = ftell(fp_binfile);
    rewind(fp_binfile);

    DEBUG(" bin_size : 0x%lx = %s\n", bin_size, printBytes(bin_size));

    // if(bin_size % width != 0){
    //     DEBUG("Bin file must me %ld bytes aligned.\n", width);
    //     return 1;
    // }

    if (bin_size > mem_size) {
        DEBUG("Bin file if too big.\n");
        return 1;
    }

    if (enable_ecc && width != 8) {
        DEBUG("Invalid width for ECC.\n");
        return 1;
    }

    uint8_t line_buffer[width];

    DEBUG("Create %ld cuts files of size %ld bytes ...\n", tensor_norm,
          cut_size);

    // Open cut files
    char path[255];
    snprintf(path, sizeof(path), "%s/main.hex", respath);
    FILE *fp_main_hexfile = fopen(path, "w");  // Open main hex file
    assert(fp_main_hexfile);
    FILE *fp_cuts[tensor_norm];
    for (size_t i = 0; i < tensor_norm; i++) {
        snprintf(path, sizeof(path), "%s/cut%03ld.hex", respath, i);
        fp_cuts[i] = fopen(path, "w");  // Open cuts
        assert(fp_cuts[i]);
    }

    // Write cut files
    for (size_t line = 0; line < num_line_total; line++) {
        // Compute cut id
        uint64_t tensor_id[tensor_size];
        uint64_t cut_id = line_to_cut_id(line, tensor, tensor_mode, tensor_size,
                                         height, tensor_id);
        FILE *fp_cut = fp_cuts[cut_id];

        // debug_tensor(tensor_id, tensor_size);
        // DEBUG("Line %ld -> CUT %ld\n", line, cut_id);

        // Read Line from input stream
        uint64_t size = fread(line_buffer, 1, sizeof(line_buffer), fp_binfile);
        if (!size) {
            break;
        }
        // Pad if needed
        if (size != sizeof(line_buffer)) {
            memset(line_buffer + size, 0, sizeof(line_buffer) - size);
        }

        // Write Line to the corresponding cut
        if (enable_ecc) {
            unsigned int eccv = ecc(*(uint64_t *)line_buffer);
            fprintf(fp_main_hexfile, "%.2x", eccv);
            fprintf(fp_cut, "%.2x", eccv);
        }
        for (signed int i = width - 1; i >= 0; i--) {  // TODO: Align
            fprintf(fp_main_hexfile, "%.2x", line_buffer[i]);
            fprintf(fp_cut, "%.2x", line_buffer[i]);
        }
        fprintf(fp_main_hexfile, "\n");
        fprintf(fp_cut, "\n");
    }

    // Close cut files
    fclose(fp_main_hexfile);
    for (size_t i = 0; i < tensor_norm; i++) {
        fclose(fp_cuts[i]);
    }

    DEBUG("Generate output(stdout) from template %s\n", template);
    FILE *fp_tcl = stdout;
    assert(fp_tcl);
    for (size_t id = 0; id < tensor_norm; id++) {
        uint64_t tensor_id[tensor_size];
        unserialise_tensor_id(id, tensor, tensor_size, tensor_id);
        assert(serialise_tensor_id(tensor_id, tensor, tensor_size) == id);

        // debug_tensor(tensor_id, tensor_size);
        // DEBUG(" <-> %ld\n", id);

        size_t idx = 0;
        for (char *c = template; *c != '\0'; c++) {
            switch (*c) {
                case '@':
                    // Filename
                    fprintf(fp_tcl, "%s/cut%03ld.hex", respath, id);
                    break;
                case '%': {
                    // ID
                    assert(idx < tensor_size);
                    fprintf(fp_tcl, "%ld", tensor_id[tensor_size - idx - 1]);
                    idx++;
                    break;
                }
                default:
                    fputc(*c, fp_tcl);
                    break;
            }
        }
        fprintf(fp_tcl, "\n");
    }

    DEBUG("Succes! Result folder is %s\n", respath);

    return 0;
}
