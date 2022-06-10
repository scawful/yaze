#include "Compression.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cstddef>
#include <iostream>
#include <memory>
#include <vector>

#define INITIAL_ALLOC_SIZE 1024

#define D_CMD_COPY 0
#define D_CMD_BYTE_REPEAT 1
#define D_CMD_WORD_REPEAT 2
#define D_CMD_BYTE_INC 3
#define D_CMD_COPY_EXISTING 4

#define D_MAX_NORMAL_length 32
#define D_max_length 1024

#define D_NINTENDO_C_MODE1 0
#define D_NINTENDO_C_MODE2 1

#define X_ std::byte {
#define _X }

#define MY_BUILD_HEADER(command, length) (command << 5) + ((length)-1)

namespace yaze {
namespace Application {
namespace Utils {

char* ALTTPCompression::DecompressGfx(const char* c_data,
                                      const unsigned int start,
                                      unsigned int max_length,
                                      unsigned int* uncompressed_data_size,
                                      unsigned int* compressed_length) {
  char* toret = std_nintendo_.Decompress(c_data, start, max_length,
                                         uncompressed_data_size,
                                         compressed_length, D_NINTENDO_C_MODE2);
  return toret;
}

char* ALTTPCompression::DecompressOverworld(
    const char* c_data, const unsigned int start, unsigned int max_length,
    unsigned int* uncompressed_data_size, unsigned int* compressed_length) {
  char* toret = std_nintendo_.Decompress(c_data, start, max_length,
                                         uncompressed_data_size,
                                         compressed_length, D_NINTENDO_C_MODE1);
  return toret;
}

char* ALTTPCompression::CompressGfx(const char* u_data,
                                    const unsigned int start,
                                    const unsigned int length,
                                    unsigned int* compressed_size) {
  return std_nintendo_.Compress(u_data, start, length, compressed_size,
                                D_NINTENDO_C_MODE2);
}

char* ALTTPCompression::CompressOverworld(const char* u_data,
                                          const unsigned int start,
                                          const unsigned int length,
                                          unsigned int* compressed_size) {
  return std_nintendo_.Compress(u_data, start, length, compressed_size,
                                D_NINTENDO_C_MODE1);
}

/*
 * The compression format follow a simple pattern:
 * first byte represente a header. The header represent a command and a length
 * The bytes after the header have meaning depending on the command
 * Then you have a new header byte and so on, until you hit a header with the
 * value FF
 */
char* StdNintendoCompression::Decompress(const char* c_data,
                                         const unsigned int start,
                                         unsigned int max_length,
                                         unsigned int* uncompressed_data_size,
                                         unsigned int* compressed_length,
                                         char mode) {
  char* u_data;
  unsigned char header;
  unsigned int c_data_pos;
  unsigned int u_data_pos;
  unsigned int allocated_memory;
  unsigned int max_offset;

  max_offset = 0;
  if (max_length != 0) max_offset = start + max_length;
  header = c_data[start];
  u_data =
      (char*)malloc(INITIAL_ALLOC_SIZE);  // No way to know the final size, we
                                          // will probably realloc if needed
  allocated_memory = INITIAL_ALLOC_SIZE;
  u_data_pos = 0;
  c_data_pos = start;

  while (header != 0xFF) {
    unsigned int length;
    char command;

    command = header >> 5;     // 3 hightest bits are the command
    length = (header & 0x1F);  // The rest is the length

    // Extended header, to allow for bigger length value than 32
    if (command == 7) {
      // The command are the next 3 bits
      command = (header >> 2) & 7;
      // 2 bits in the original header are the hight bit for the new length
      // the next byte is added to this length

      length =
          ((int)((header & 3) << 8)) + (unsigned char)c_data[c_data_pos + 1];
      c_data_pos++;
    }

    // length value starts at 0, 0 is 1
    length++;
    printf("%d[%d]", command, length);
    printf("header %02X - Command : %d , length : %d\n", header, command,
           length);
    if (c_data_pos >= max_offset && max_offset != 0) {
      decompression_error_ =
          "Compression string exceed the max_length specified";
      goto error;
    }

    if (u_data_pos + length + 1 > allocated_memory)  // Adjust allocated memory
    {
      printf("Memory get reallocated by %d was %d\n", INITIAL_ALLOC_SIZE,
             allocated_memory);
      u_data = (char*)realloc(u_data, allocated_memory + INITIAL_ALLOC_SIZE);
      if (u_data == NULL) {
        decompression_error_ = "Can't realloc memory";
        return NULL;
      }
      allocated_memory += INITIAL_ALLOC_SIZE;
    }

    switch (command) {
      case D_CMD_COPY: {  // No compression, data are copied as
        if (max_offset != 0 && c_data_pos + 1 + length > max_offset) {
          // decompression_error_ = vasprintf("A copy command exceed the
          // available data %d > %d (max_length specified)\n", c_data_pos + 1 +
          // length, max_offset);
          goto error;
        }
        memcpy(u_data + u_data_pos, c_data + c_data_pos + 1, length);
        c_data_pos += length + 1;
        break;
      }
      case D_CMD_BYTE_REPEAT: {  // Copy the same byte length time
        memset(u_data + u_data_pos, c_data[c_data_pos + 1], length);
        c_data_pos += 2;
        break;
      }
      case D_CMD_WORD_REPEAT: {  // Next byte is A, the one after is B, copy the
                                 // sequence AB length times
        char a = c_data[c_data_pos + 1];
        char b = c_data[c_data_pos + 2];
        for (int i = 0; i < length; i = i + 2) {
          u_data[u_data_pos + i] = a;
          if ((i + 1) < length) u_data[u_data_pos + i + 1] = b;
        }
        c_data_pos += 3;
        break;
      }
      case D_CMD_BYTE_INC: {  // Next byte is copied and incremented length time
        for (int i = 0; i < length; i++) {
          u_data[u_data_pos + i] = c_data[c_data_pos + 1] + i;
        }
        c_data_pos += 2;
        break;
      }
      case D_CMD_COPY_EXISTING: {  // Next 2 bytes form an offset to pick data
                                   // from the output
        // printf("%02X,%02X\n", (unsigned char) c_data[c_data_pos + 1],
        // (unsigned char) c_data[c_data_pos + 2]);
        unsigned short offset;
        if (mode == D_NINTENDO_C_MODE2)
          offset = (unsigned char)(c_data[c_data_pos + 1]) |
                   ((unsigned char)(c_data[c_data_pos + 2]) << 8);
        if (mode == D_NINTENDO_C_MODE1)
          offset = (unsigned char)(c_data[c_data_pos + 2]) |
                   ((unsigned char)(c_data[c_data_pos + 1]) << 8);
        if (offset > u_data_pos) {
          printf(
              "Offset for command copy existing is larger than the current "
              "position (Offset : 0x%04X | Pos : 0x%06X\n",
              offset, u_data_pos);
          goto error;
        }
        if (u_data_pos + length >= allocated_memory) {
          printf("Memory get reallocated by a copy,  %d was %d\n",
                 INITIAL_ALLOC_SIZE, allocated_memory);
          u_data =
              (char*)realloc(u_data, allocated_memory + INITIAL_ALLOC_SIZE);
          if (u_data == NULL) {
            decompression_error_ = "Can't realloc memory";
            return NULL;
          }
          allocated_memory += INITIAL_ALLOC_SIZE;
        }
        memcpy(u_data + u_data_pos, u_data + offset, length);
        c_data_pos += 3;
        break;
      }
      default: {
        decompression_error_ =
            "Invalid command in the header for decompression";
        goto error;
      }
    }
    u_data_pos += length;
    // printf("%d|%d\n", c_data_pos, u_data_pos);
    header = c_data[c_data_pos];
  }
  *uncompressed_data_size = u_data_pos;
  *compressed_length = c_data_pos + 1;
  // printf("\n");
  return u_data;
  // yay goto usage :)
error:
  free(u_data);
  return NULL;
}

void StdNintendoCompression::PrintComponent(CompressionComponent* piece) {
  printf("Command : %d\n", piece->command);
  printf("length  : %d\n", piece->length);
  printf("Argument length : %d\n", piece->argument_length);
  auto size = piece->argument_length;
  auto str = piece->argument;
  char* toret = new char[size * 3 + 1];
  unsigned int i;
  for (i = 0; i < size; i++) {
    sprintf(toret + i * 3, "%02X ", (unsigned char)str[i]);
  }
  toret[size * 3] = 0;

  printf("Argument :%s\n", toret);
}

StdNintendoCompression::CompressionComponent*
StdNintendoCompression::CreateComponent(const char command,
                                        const unsigned int length,
                                        const char* args,
                                        const unsigned int argument_length) {
  CompressionComponent* toret =
      (CompressionComponent*)malloc(sizeof(CompressionComponent));
  toret->command = command;
  toret->length = length;
  if (args != NULL) {
    toret->argument = (char*)malloc(argument_length);
    memcpy(toret->argument, args, argument_length);
  } else
    toret->argument = NULL;
  toret->argument_length = argument_length;
  toret->next = NULL;
  return toret;
}

void StdNintendoCompression::DestroyComponent(CompressionComponent* piece) {
  free(piece->argument);
  free(piece);
}

void StdNintendoCompression::DestroyChain(CompressionComponent* piece) {
  while (piece != NULL) {
    CompressionComponent* p = piece->next;
    DestroyComponent(piece);
    piece = p;
  }
}

// Merge consecutive copy if possible
StdNintendoCompression::CompressionComponent*
StdNintendoCompression::merge_copy(CompressionComponent* start) {
  CompressionComponent* piece = start;

  while (piece != NULL) {
    if (piece->command == D_CMD_COPY && piece->next != NULL &&
        piece->next->command == D_CMD_COPY) {
      if (piece->length + piece->next->length <= D_max_length) {
        unsigned int previous_length = piece->length;
        piece->length = piece->length + piece->next->length;
        // piece->argument = realloc(piece->argument, piece->length);
        piece->argument_length = piece->length;
        memcpy(piece->argument + previous_length, piece->next->argument,
               piece->next->argument_length);
        printf("-Merged copy created\n");
        PrintComponent(piece);
        CompressionComponent* p_next_next = piece->next->next;
        DestroyComponent(piece->next);
        piece->next = p_next_next;
        continue;  // Next could be another copy
      }
    }
    piece = piece->next;
  }
  return start;
}

unsigned int StdNintendoCompression::create_compression_string(
    CompressionComponent* start, char* output, char mode) {
  unsigned int pos = 0;
  CompressionComponent* piece = start;

  while (piece != NULL) {
    if (piece->length <= D_MAX_NORMAL_length)  // Normal header
    {
      output[pos++] = MY_BUILD_HEADER(piece->command, piece->length);
    } else {
      if (piece->length <= D_max_length) {
        output[pos++] = (7 << 5) | ((unsigned char)piece->command << 2) |
                        (((piece->length - 1) & 0xFF00) >> 8);
        printf("Building extended header : cmd: %d, length: %d -  %02X\n",
               piece->command, piece->length, (unsigned char)output[pos - 1]);
        output[pos++] = (char)((piece->length - 1) & 0x00FF);
      } else {  // We need to split the command
        unsigned int length_left = piece->length - D_max_length;
        piece->length = D_max_length;
        CompressionComponent* new_piece = NULL;
        if (piece->command == D_CMD_BYTE_REPEAT ||
            piece->command == D_CMD_WORD_REPEAT) {
          new_piece = CreateComponent(piece->command, length_left,
                                      piece->argument, piece->argument_length);
        }
        if (piece->command == D_CMD_BYTE_INC) {
          new_piece = CreateComponent(piece->command, length_left,
                                      piece->argument, piece->argument_length);
          new_piece->argument[0] = (char)(piece->argument[0] + D_max_length);
        }
        if (piece->command == D_CMD_COPY) {
          piece->argument_length = D_max_length;
          new_piece =
              CreateComponent(piece->command, length_left, NULL, length_left);
          memcpy(new_piece->argument, piece->argument + D_max_length,
                 length_left);
        }
        if (piece->command == D_CMD_COPY_EXISTING) {
          piece->argument_length = D_max_length;
          unsigned int offset = piece->argument[0] + (piece->argument[1] << 8);
          new_piece = CreateComponent(piece->command, length_left,
                                      piece->argument, piece->argument_length);
          if (mode == D_NINTENDO_C_MODE2) {
            new_piece->argument[0] = (offset + D_max_length) & 0xFF;
            new_piece->argument[1] = (offset + D_max_length) >> 8;
          }
          if (mode == D_NINTENDO_C_MODE1) {
            new_piece->argument[1] = (offset + D_max_length) & 0xFF;
            new_piece->argument[0] = (offset + D_max_length) >> 8;
          }
        }
        printf("New added piece\n");
        PrintComponent(new_piece);
        new_piece->next = piece->next;
        piece->next = new_piece;
        continue;
      }
    }
    if (piece->command == D_CMD_COPY_EXISTING) {
      char tmp[2];
      if (mode == D_NINTENDO_C_MODE2) {
        tmp[0] = piece->argument[0];
        tmp[1] = piece->argument[1];
      }
      if (mode == D_NINTENDO_C_MODE1) {
        tmp[0] = piece->argument[1];
        tmp[1] = piece->argument[0];
      }
      memcpy(output + pos, tmp, 2);
    } else {
      memcpy(output + pos, piece->argument, piece->argument_length);
    }
    pos += piece->argument_length;
    piece = piece->next;
  }
  output[pos] = 0xFF;
  return pos + 1;
}

// TODO TEST compressed data border for each cmd

char* StdNintendoCompression::Compress(const char* u_data,
                                       const unsigned int start,
                                       const unsigned int length,
                                       unsigned int* compressed_size,
                                       char mode) {
  // we will realloc later
  char* compressed_data = (char*)malloc(
      length +
      10);  // Worse case should be a copy of the string with extended header
  CompressionComponent* compressed_chain = CreateComponent(1, 1, "aaa", 2);
  CompressionComponent* compressed_chain_start = compressed_chain;

  unsigned int u_data_pos = start;
  unsigned int last_pos = start + length - 1;
  printf("max pos :%d\n", last_pos);
  // unsigned int previous_start = start;
  unsigned int data_size_taken[5] = {0, 0, 0, 0, 0};
  unsigned int cmd_size[5] = {0, 1, 2, 1, 2};
  char cmd_args[5][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}, {0, 0}};
  unsigned int bytes_since_last_compression =
      0;  // Used when skipping using copy

  while (1) {
    memset(data_size_taken, 0, sizeof(data_size_taken));
    memset(cmd_args, 0, sizeof(cmd_args));
    printf("Testing every command\n");

    /* We test every command to see the gain with current position */
    {  // BYTE REPEAT
      printf("Testing byte repeat\n");
      unsigned int pos = u_data_pos;
      char byte_to_repeat = u_data[pos];
      while (pos <= last_pos && u_data[pos] == byte_to_repeat) {
        data_size_taken[D_CMD_BYTE_REPEAT]++;
        pos++;
      }
      cmd_args[D_CMD_BYTE_REPEAT][0] = byte_to_repeat;
    }
    {  // WORD REPEAT
      printf("Testing word repeat\n");
      if (u_data_pos + 2 <= last_pos &&
          u_data[u_data_pos] != u_data[u_data_pos + 1]) {
        unsigned int pos = u_data_pos;
        char byte1 = u_data[pos];
        char byte2 = u_data[pos + 1];
        pos += 2;
        data_size_taken[D_CMD_WORD_REPEAT] = 2;
        while (pos + 1 <= last_pos) {
          if (u_data[pos] == byte1 && u_data[pos + 1] == byte2)
            data_size_taken[D_CMD_WORD_REPEAT] += 2;
          else
            break;
          pos += 2;
        }
        cmd_args[D_CMD_WORD_REPEAT][0] = byte1;
        cmd_args[D_CMD_WORD_REPEAT][1] = byte2;
      }
    }
    {  // INC BYTE
      printf("Testing byte inc\n");
      unsigned int pos = u_data_pos;
      char byte = u_data[pos];
      pos++;
      data_size_taken[D_CMD_BYTE_INC] = 1;
      while (pos <= last_pos && ++byte == u_data[pos]) {
        data_size_taken[D_CMD_BYTE_INC]++;
        pos++;
      }
      cmd_args[D_CMD_BYTE_INC][0] = u_data[u_data_pos];
    }
    {  // INTRA CPY
      printf("Testing intra copy\n");
      if (u_data_pos != start) {
        unsigned int searching_pos = start;
        // unsigned int compressed_length = u_data_pos - start;
        unsigned int current_pos_u = u_data_pos;
        unsigned int copied_size = 0;
        unsigned int search_start = start;
        /*printf("Searching for : ");
         for (unsigned int i = 0; i < 8; i++)
         {
             printf("%02X ", (unsigned char) u_data[u_data_pos + i]);
         }
         printf("\n");*/
        while (searching_pos < u_data_pos && current_pos_u <= last_pos) {
          while (u_data[current_pos_u] != u_data[searching_pos] &&
                 searching_pos < u_data_pos)
            searching_pos++;
          search_start = searching_pos;
          while (current_pos_u <= last_pos &&
                 u_data[current_pos_u] == u_data[searching_pos] &&
                 searching_pos < u_data_pos) {
            copied_size++;
            current_pos_u++;
            searching_pos++;
          }
          if (copied_size > data_size_taken[D_CMD_COPY_EXISTING]) {
            search_start -= start;
            printf("-Found repeat of %d at %d\n", copied_size, search_start);
            data_size_taken[D_CMD_COPY_EXISTING] = copied_size;
            cmd_args[D_CMD_COPY_EXISTING][0] = search_start & 0xFF;
            cmd_args[D_CMD_COPY_EXISTING][1] = search_start >> 8;
          }
          current_pos_u = u_data_pos;
          copied_size = 0;
        }
      }
    }
    printf("Finding the best gain\n");
    // We check if a command managed to pick up 2 or more bytes
    // We don't want to be even with copy, since it's possible to merge copy
    unsigned int max_win = 2;
    unsigned int cmd_with_max = D_CMD_COPY;
    for (unsigned int cmd_i = 1; cmd_i < 5; cmd_i++) {
      unsigned int cmd_size_taken = data_size_taken[cmd_i];
      if (cmd_size_taken > max_win && cmd_size_taken > cmd_size[cmd_i] &&
          !(cmd_i == D_CMD_COPY_EXISTING &&
            cmd_size_taken == 3)  // FIXME: Should probably be a
                                  // table that say what is even with copy
                                  // but all other cmd are 2
      ) {
        printf("--C:%d / S:%d\n", cmd_i, cmd_size_taken);
        cmd_with_max = cmd_i;
        max_win = cmd_size_taken;
      }
    }
    if (cmd_with_max == D_CMD_COPY)  // This is the worse case
    {
      printf("- Best command is copy\n");
      // We just move through the next byte and don't 'compress' yet, maybe
      // something is better after.
      u_data_pos++;
      bytes_since_last_compression++;
      if (bytes_since_last_compression == 32 ||
          u_data_pos > last_pos)  // Arbitraty choice to do a 32 bytes grouping
      {
        char buffer[32];
        memcpy(buffer, u_data + u_data_pos - bytes_since_last_compression,
               bytes_since_last_compression);
        CompressionComponent* new_comp_piece =
            CreateComponent(D_CMD_COPY, bytes_since_last_compression, buffer,
                            bytes_since_last_compression);
        compressed_chain->next = new_comp_piece;
        compressed_chain = new_comp_piece;
        bytes_since_last_compression = 0;
      }
    } else {  // Yay we get something better
      printf("- Ok we get a gain from %d\n", cmd_with_max);
      char buffer[2];
      buffer[0] = cmd_args[cmd_with_max][0];
      if (cmd_size[cmd_with_max] == 2) buffer[1] = cmd_args[cmd_with_max][1];
      CompressionComponent* new_comp_piece = CreateComponent(
          cmd_with_max, max_win, buffer, cmd_size[cmd_with_max]);
      if (bytes_since_last_compression !=
          0)  // If we let non compressed stuff, we need to add a copy chuck
              // before
      {
        char* copy_buff = (char*)malloc(bytes_since_last_compression);
        memcpy(copy_buff, u_data + u_data_pos - bytes_since_last_compression,
               bytes_since_last_compression);
        CompressionComponent* copy_chuck =
            CreateComponent(D_CMD_COPY, bytes_since_last_compression, copy_buff,
                            bytes_since_last_compression);
        compressed_chain->next = copy_chuck;
        compressed_chain = copy_chuck;
      }
      compressed_chain->next = new_comp_piece;
      compressed_chain = new_comp_piece;
      u_data_pos += max_win;
      bytes_since_last_compression = 0;
    }
    if (u_data_pos > last_pos) break;
    if (std_nintendo_compression_sanity_check &&
        compressed_chain_start->next != NULL) {
      // We don't call merge copy so we need more space
      char* tmp = (char*)malloc(length * 2);
      *compressed_size =
          create_compression_string(compressed_chain_start->next, tmp, mode);
      unsigned int p;
      unsigned int k;
      char* uncomp = Decompress(tmp, 0, 0, &p, &k, mode);
      if (uncomp == NULL) {
        fprintf(stderr, "%s\n", decompression_error_);
        return NULL;
      }

      free(tmp);
      if (memcmp(uncomp, u_data + start, p) != 0) {
        printf("Compressed data does not match uncompressed data at %d\n",
               (unsigned int)(u_data_pos - start));
        free(uncomp);
        DestroyChain(compressed_chain_start);
        return NULL;
      }
      free(uncomp);
    }
  }
  // First is a dumb place holder
  merge_copy(compressed_chain_start->next);

  *compressed_size = create_compression_string(compressed_chain_start->next,
                                               compressed_data, mode);
  DestroyChain(compressed_chain_start);
  return compressed_data;
}

}  // namespace Utils
}  // namespace Application
}  // namespace yaze
