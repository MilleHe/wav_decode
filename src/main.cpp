/*#############################################################################
#
#            Notice of Copyright and Non-Disclosure Information
#
#
###############################################################################
*/

#include "common.h"

#include <memory>
#include <assert.h>


//#############################################################################
#define SERVICE_NAME                          "Wave Decode"

#define DEFAULT_FILE_PATH                     "Decoding_WaveFiles/file_1.wav"
#define DEFAULT_FILE_PATH2                    "Decoding_WaveFiles/file_2.wav"
#define DEFAULT_FILE_PATH3                    "Decoding_WaveFiles/file_3.wav"

//#############################################################################
#define SIGNAL_THRESHOLD_US                   320

#define SIGNAL_START_US                       2500000
#define SIGNAL_END_US                         500000

#define MESSAGE_START_INDICATOR               0x42
#define MESSAGE_START_INDICATOR2              0x03
#define MESSAGE_END_INDICATOR                 0

#define MESSAGE_UNIT_COUNT                    30

#define BIT_ARRAY_ACCESS(array, index)   ((array[(index)/8]>>((index)%8)) & 1)
#define BIT_ARRAY_ASSIGN(array, index)   (array[(index)/8] |= (1<<((index)%8)))



//#############################################################################
// test function to save array value or bit to file for examination
void store_data(
    const std::shared_ptr<uint8_t[]>& array, 
    const size_t& array_count, 
    const uint8_t& array_type)
  {
  if (array.use_count() == 0)
    return;

  // open file due to different type
  FILE* file = NULL;
  if (array_type == 1)
    file = fopen("./1.sample_store", "w+");
  else if (array_type == 2)
    file = fopen("./2.bit_store", "w+");
  else if (array_type == 3)
    file = fopen("./3.byte_store", "w+");
  else
    assert(0);

  for (size_t i = 0; i < array_count; i++)
    {
    if (array_type == 1 || array_type == 2)
      // print bits
      for (uint8_t j = 0; j < 8; j++)
        fprintf(file , "%d\n", (int)(array[i] >> j & 1));
    else if (array_type == 3)
      // print bytes
      fprintf(file , "%d\n", (int)array[i]);
    }

  fclose(file);
  file = NULL;
  }



//#############################################################################
// load samples from wav file
bool load_samples(
    const std::string& file_path, 
    std::shared_ptr<uint8_t[]>& samples,
    size_t& sample_count,
    size_t& samples_per_rectangle)
  {
  // parameter check
  if (file_path.empty())
    return false;

  // open file
  FILE* file = fopen(file_path.data(), "r");
  if (file == NULL)
    {
    tprint("Fail to open file '%s'", file_path.data());
    return false;
    }

  //###########################################################################
  struct wav_header
    {
    /* RIFF Chunk Descriptor */
    uint8_t         RIFF[4];          // RIFF Header Magic header
    uint32_t        chunk_size;       // RIFF Chunk Size
    uint8_t         WAVE[4];          // WAVE Header
    /* "fmt" sub-chunk */
    uint8_t         fmt_chunk[4];     // FMT header
    uint32_t        fmt_chunk_size;   // Size of the fmt chunk
    uint16_t        audio_format;     // Audio format
    uint16_t        channel_number;   // Number of channels 1=Mono 2=Sterio
    uint32_t        sample_rate;      // Sampling Frequency in Hz
    uint32_t        bytes_per_sec;    // bytes per second
    uint16_t        bytes_per_block;  // 2=16-bit mono, 4=16-bit stereo
    uint16_t        bits_per_sample;  // Number of bits per sample
    /* "data" sub-chunk */
    uint8_t         data_chunk[4];    // "data"  string
    uint32_t        data_chunk_size;  // Sampled data length
    } header;

  // load wav header for information
  if (fread(&header, 1, sizeof(struct wav_header), file) == 0)
    {
    tprint("Read file error '%s'", file_path.data());
    
    fclose(file);
    file = NULL;
    return false;
    }

  // simple file format check according to header
  if (std::string((char*)header.RIFF).find("RIFF") != 0 || 
      std::string((char*)header.WAVE).find("WAVE") != 0)
    {
    tprint("File might not be WAV: '%s':'%s'", header.RIFF, header.WAVE);
    
    fclose(file);
    file = NULL;
    return false;
    }
  
  // Calculate and display useful information from the header
  samples_per_rectangle = 
    header.sample_rate * SIGNAL_THRESHOLD_US / 1000000;
  uint32_t total_sample_count = header.data_chunk_size / 
    (header.channel_number * header.bits_per_sample / 8);
  sample_count = total_sample_count - 
    ((SIGNAL_START_US+SIGNAL_END_US-10000.0)/1000000.0*header.sample_rate);
  // allow 10000us tolerance
  sample_count -= (sample_count % 8);     // round by 8

  printf("File '%s' loaded\n"
    "Number of channel           : %d\n" 
    "Data size                   : %d\n"
    "Total samples               : %d\n"
    "Samples between lead & end  : %ld\n"
    "Samples per sec             : %d\n"
    "Samples per rectangle       : %ld\n"
    "Bytes per sec               : %d\n", 
      file_path.data(), header.channel_number, header.data_chunk_size,
      total_sample_count, sample_count, header.sample_rate,   
      samples_per_rectangle, header.bytes_per_sec);

  // The signal starts with a lead tone of roughly 2.5 seconds
  fseek(file, size_t(SIGNAL_START_US/1000000.0*header.bytes_per_sec), SEEK_CUR);

  // every byte store 8 bits
  samples.reset(new uint8_t[sample_count/8]{0});

  // load samples from file
  uint16_t sample = 0;
  for (uint32_t i = 0; i < sample_count; i++)
    {
    if (fread(&sample, 1, 2, file) != 2)
      assert(0);
    
    // Flat samples to be 0 or 1 and store by bit
    // samples array initiated as 0, so only store flatted '1'
    if (sample >> 15)
      BIT_ARRAY_ASSIGN(samples, i);
    
    // Assumption: sample values in all channels are same for valid signals.
    if (header.channel_number > 1)
      // skip data in other channels
      fseek(file, (header.channel_number-1) * 2, SEEK_CUR);
    }

  // close file
  fclose(file);
  file = NULL;

  return true;
  }

//#############################################################################
// convert loaded samples to bits, Audio Frequency Shift-Keying (AFSK)
bool samples_to_bits(
    const std::shared_ptr<uint8_t[]>& samples,
    const size_t& sample_count,
    const size_t& samples_per_rectangle,
    std::shared_ptr<uint8_t[]>& bits,
    size_t& bit_count)
  {
  // parameter check
  if (samples.use_count() == 0 || sample_count < samples_per_rectangle)
    return false;
  
  bits.reset(new uint8_t[sample_count/(samples_per_rectangle*8)]{0});
  bit_count = 0;

  uint8_t prev_match = 0;
  bool prev_len_tolerated = false;
  int8_t prev_matched_rectangle_value = -1;     // -1 to init, 0/1
  bool current_rectangle_value = false;
  uint8_t current_rectangle_len = 0;
  bool rectangle_mismatch = false;
  int8_t rectangle_head = 0;
  const size_t sample_limit = sample_count - samples_per_rectangle + 1;


  /****************************************************************************
   * An idea for performance improvement is multithread. 
     Split samples array into parts for each thread to process. After first   
     rectangle, as the sizes of rectangle is known, boundary should not be a 
     problem among threads.
     But, it seems a bit over engineered for this interview test.
   */

  for (size_t i = 0; i < sample_limit; /*i internal incremental*/)
    {
    //#########################################################################
    // rectangle comparison
    // within a rectangle, compare reverse from end, to previous matched end
    // prev_match at   |,                  j starts
    //          {m,m,m,x,x,x,x,x,x,x,x,x,x,x}
    rectangle_mismatch = false;
    if (prev_len_tolerated)
      // no more tolerance
      current_rectangle_len = samples_per_rectangle; 
    else
      // allow 1 bit of tolerance
      current_rectangle_len = samples_per_rectangle - 1;

    //                   prev       [no /      has]      match
    rectangle_head = prev_match < 2 ? 0 : prev_match - 1;
    for (int8_t j = current_rectangle_len - 2;  j >= rectangle_head; j--)
      {
      // compare reversely
      if (BIT_ARRAY_ACCESS(samples, i+j) != BIT_ARRAY_ACCESS(samples, i+j+1))
        {
        // mismatch occurs
        rectangle_mismatch = true;
        prev_match = current_rectangle_len - j -1;
        break;
        }
      } // j loop

    // mismatch occurs, step to next rectangle compare
    if (rectangle_mismatch)
      {
      prev_matched_rectangle_value = -1;
      if (prev_len_tolerated)
        prev_len_tolerated = false;
      i += (current_rectangle_len - prev_match);
      //                            this has been changed as current match

      continue; // i loop
      }

    //#########################################################################
    // this rectangle makes a bit, or part of 0 bit

    if (prev_len_tolerated)
      // this rectangle did not tolerate
      prev_len_tolerated = false;
    else
    // previous rectangle did not tolerate, this can
      {
      if (BIT_ARRAY_ACCESS(samples, i + current_rectangle_len - 1) !=  
            BIT_ARRAY_ACCESS(samples, i + current_rectangle_len))
        // this rectangle needs tolerance
        prev_len_tolerated = true;
      else
        // this rectangle does not need tolerance as well
        current_rectangle_len++;
      }
      
    // decide bit value of this rectangle
    current_rectangle_value = BIT_ARRAY_ACCESS(samples, i);
    if (prev_matched_rectangle_value == current_rectangle_value)
      {
      // 0 bit, but no need to change samples as all bits initiated 0

      bit_count++;
      prev_matched_rectangle_value = -1;
      }
    else if (prev_matched_rectangle_value != -1)
      {
      // 1 bit, from previous rectangle
      BIT_ARRAY_ASSIGN(bits, bit_count);
      bit_count++;
      prev_matched_rectangle_value = current_rectangle_value;
      }
    else
      {
      // not sure this rectangle to be a 1 bit or first half of 0 bit
      assert(prev_matched_rectangle_value == -1);
      prev_matched_rectangle_value = current_rectangle_value;
      } 

    prev_match = 0;
    i += current_rectangle_len;  
    } // i loop
    
  // an extra 1 bit is confirmed
  if (prev_matched_rectangle_value != -1)
    {
    BIT_ARRAY_ASSIGN(bits, bit_count);
    bit_count++;
    }

  return true;
  }

//#############################################################################
// convert bits into bytes
bool bits_to_bytes(
    const std::shared_ptr<uint8_t[]>& bits, 
    const size_t& bit_count, 
    std::shared_ptr<uint8_t[]>& bytes,
    size_t& byte_count)
  {
  // parameter check
  if (bits.use_count() == 0 || bit_count < 8)
    return false;
 
  // init bytes array 
  byte_count = 0;
  bytes.reset(new uint8_t[bit_count/11]{0});
  // remaining (bit_count%11) bits would not make a byte
 
  // declare and init loop variables
  const size_t bits_limit = bit_count - 10 - bit_count%11;
  bool message_start = false;
  uint8_t byte_temp = 0;

  // bits loop
  for (size_t i = 0; i < bits_limit; i++)
    {
    // unmatch pattern
    if (BIT_ARRAY_ACCESS(bits, i) ||      // bit 1  : !0
        !BIT_ARRAY_ACCESS(bits, i+9) ||   // bit 10 : !1
        !BIT_ARRAY_ACCESS(bits, i+10))    // bit 11 : !1
      continue;

    // convert 8 bits into 1 byte
    // pitifully cannot uint8_t* to bits{i+1} mem address, reverse bit alignment
    byte_temp = 0;
    for (uint8_t j = 0; j < 8; j++)
      // byte_temp init 0, only assign 1 bit
      if (BIT_ARRAY_ACCESS(bits, i+j+1))
        byte_temp |= 1 << j;

    i += 10;

    // The first two bytes are 0x42 and 0x03
    // only check one indicator
    if (!message_start && byte_temp == MESSAGE_START_INDICATOR)
      message_start = true;
    
    if (!message_start)
      continue;

    bytes[byte_count++] = byte_temp;  
    
    // The last byte before the end block is a 0x00 byte
    if (byte_temp == MESSAGE_END_INDICATOR)
      break;
    }

  return true;
  }

//#############################################################################
// checksum of bytes
bool checksum_message(
    const std::shared_ptr<uint8_t[]>& bytes, const size_t& byte_count)
  {
  // parameter check
  if (bytes.use_count() == 0 || byte_count < 4)
    return false;

  // declare loop variables
  bool message_start = false;
  uint8_t checksum = 0;
  
  // bytes loop
  for (size_t i = 0; i < byte_count; i++)
    {
    // The first two bytes are 0x42 and 0x03
    if (!message_start && bytes[i] == MESSAGE_START_INDICATOR)
      {
      assert(i+1 < byte_count);
      if (bytes[i+1] == MESSAGE_START_INDICATOR2)
        {
        message_start = true;
        i++;
        }

      continue;
      }
    // stop if message end indicator
    else if (bytes[i] == MESSAGE_END_INDICATOR)
      break;

    if (!message_start)
      continue;

    assert(i+MESSAGE_UNIT_COUNT < byte_count);
    checksum = 0;
    for (size_t j = 0; j < MESSAGE_UNIT_COUNT; j++)
      checksum += bytes[i+j];

    if (checksum != bytes[i+MESSAGE_UNIT_COUNT])
      {
      tprint("Checksum fails at message index %ld", i);
      return false;
      }

    i += MESSAGE_UNIT_COUNT;
    }

  return true;
  }



//#############################################################################
int main(int argc, char** argv)
  {
  tprint("Service '%s' starts", SERVICE_NAME);
  
  std::string filepath("");

  //###########################################################################
  // parameter parse
  if (argc > 1)
    {
    if (std::string(argv[1]) == "1")
      {
      filepath = DEFAULT_FILE_PATH;
      }
    else if (std::string(argv[1]) == "2")
      {
      filepath = DEFAULT_FILE_PATH2;
      }
    else if (std::string(argv[1]) == "3")
      {
      filepath = DEFAULT_FILE_PATH3;
      }
    else if (std::string(argv[1]) == "h" || std::string(argv[1]) == "help")
      {
      printf(
        "  h|help         For this help.\n"
        "  1|2|3          "
          "File './Decoding_WaveFiles/file_1|2|3.wav' would be loaded.\n"
        "  file_path      File path provided would be loaded.\n");
      tprint("Service '%s' ends", SERVICE_NAME);
      return 0;
      }
    else
      {
      filepath = std::string(argv[1]);
      }
    }

  // Use default file if not provided
  if (filepath.empty())
    {
    tprint("No file path provided, use default '%s'", DEFAULT_FILE_PATH);
    filepath = DEFAULT_FILE_PATH;
    }
  else
    {
    tprint("Use data file '%s'", filepath.data());
    }

  //###########################################################################
  // Load file data to sample array
  std::shared_ptr<uint8_t[]> samples;
  // Ideal container should be bitset. However std::bitset does not allow 
  //   dynamic size and boost::dynamic_bitset would introduce extra library.
  //   And this bits container still uses much less memory than bool array.
  
  size_t sample_count = 0;
  size_t samples_per_rectangle = 0;
  if (!load_samples(
         filepath, samples, sample_count, samples_per_rectangle) ||
      samples.use_count() == 0)
    {
    tprint("Fail to load samples from file");
    return 1;
    }
  
  tprint("Samples are loaded");

  // store samples for examination
  // store_data(samples, sample_count/8, 1);

  //###########################################################################
  // Decode sample array to bits array
  std::shared_ptr<uint8_t[]> bits;

  size_t bit_count = 0;
  if (!samples_to_bits(
         samples, sample_count, samples_per_rectangle, bits, bit_count) ||
      bits.use_count() == 0)
    {
    tprint("Fail to decode samples into bits");
    return 1;
    }

  // release sample memory
  samples.reset();
  tprint("Bits are converted from samples");
    
  // store bits for examination
  // store_data(bits, bit_count/8, 2);

  //###########################################################################
  // Convert bits array to byte messages
  std::shared_ptr<uint8_t[]> bytes;
  size_t byte_count = 0;
  if (!bits_to_bytes(bits, bit_count, bytes, byte_count) ||
      bytes.use_count() == 0)
    {
    tprint("Fail to decode messages");    
    return 1;
    }

  // release bit memory
  bits.reset();
  tprint("Bytes are converted from bits");

  // store byte messages
  // store_data(bytes, byte_count, 3);

  //###########################################################################
  // Checksum bytes message
  if (!checksum_message(bytes, byte_count))
    {
    tprint("Fail of checksum");
    return 1;
    }

  tprint("Messages are checked and saved");


  /****************************************************************************
   * As the sizes and counts of samples, bits and bytes could all be estimated,
     I understand the previous processes could be merged to reduce iteration
     for efficiency improvement. But the codes would look complicated and 
     messy. I prefer to keep it more readable and tidy for interviewer and 
     myself in this interview but real life.
   */

  tprint("Success");

  tprint("Service '%s' ends", SERVICE_NAME);
  return 0;
  }
