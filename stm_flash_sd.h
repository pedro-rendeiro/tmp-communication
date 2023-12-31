#ifndef _STM_FLASH_SD_H
#define _STM_FLASH_SD_H

#include "FS.h"
#include "stm_pro_mode.h"
/**
 * @brief Write the code into the flash memory of STM32Fxx
 * 
 * The data from the .bin file is written into the flash memory 
 * of the client, block-by-block 
 * 
 * @param flash_file File pointer of the .bin file to be flashed
 *   
 * @return ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t WriteTaskSD(fs::File flash_file);

/**
 * @brief Read the flash memory of the STM32Fxx, for verification
 * 
 * It reads the flash memory of the STM32 block-by-block and 
 * checks it with the data from the file (with pointer passed)
 * 
 * @param flash_file File pointer of the .bin file to be verified against
 *   
 * @return ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t ReadTaskSD(fs::File flash_file);

/**
 * @brief Flash the .bin file passed, to STM32Fxx, with read verification
 * 
 * @param file_name name of the .bin to be flashed
 *   
 * @return ESP_OK - success, ESP_FAIL - failed
 */
esp_err_t SD_STM(fs::FS &fs, const char *file_path);

#endif
