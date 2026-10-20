/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Paper Counter - Giant UI Centered & Pure Linear Math & Undo Cal
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "fdc2214.h"
#include "OLED.h" 
#include <stdio.h>
#include <math.h>

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

#ifdef __GNUC__
  #define PUTCHAR_PROTOTYPE int __io_putchar(int ch)
#else
  #define PUTCHAR_PROTOTYPE int fputc(int ch, FILE *f)
#endif

PUTCHAR_PROTOTYPE
{
  extern UART_HandleTypeDef huart1; 
  HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, 0xFFFF);
  return ch;
}

void Buzz_Beep(uint16_t ms);

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */

// ==========================================
// 核心配置与算法变量
// ==========================================
#define CAL_POINTS 26  
#define SHORT_CIRCUIT_RAW_THRESHOLD 80000000 

const uint16_t Sheet_Nodes[CAL_POINTS] = {0, 1, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 60, 65, 70, 75, 80, 85, 90, 95, 100, 105, 110, 115, 120};

long long Cal_Data_Delta[CAL_POINTS] = {0}; 
uint32_t Air_Base_Raw = 0; 

// 系统状态机
typedef enum {
    STATE_CALIBRATING = 0, 
    STATE_READY,           
    STATE_MEASURED
} SystemState_t;

SystemState_t sys_state = STATE_CALIBRATING;
uint8_t cal_step = 0; 
uint8_t valid_cal_points = CAL_POINTS; 

// ==========================================
// 算法1：去极值平均滤波法
// ==========================================
uint32_t Get_Filtered_FDC_Data(void) {
    uint32_t max_val = 0;
    uint32_t min_val = 0xFFFFFFFF;
    uint64_t sum = 0;
    uint32_t temp = 0;
    
    for(int i = 0; i < 22; i++) {
        temp = FDC2214_ReadCH0_Raw(); 
        if (temp > max_val) max_val = temp;
        if (temp < min_val) min_val = temp;
        sum += temp;
        HAL_Delay(5); 
    }
    
    sum = sum - max_val - min_val; 
    return (uint32_t)(sum / 20); 
}

// ==========================================
// 算法2：分段线性插值与尾部外推 (两点纯线性占比)
// ==========================================
uint16_t Calculate_Sheets(long long current_delta) {
    if (valid_cal_points < 2) return 0;
    uint8_t is_ascending = (Cal_Data_Delta[valid_cal_points-1] > Cal_Data_Delta[1]); 

    if (is_ascending) {
        if (current_delta <= Cal_Data_Delta[1]) return Sheet_Nodes[1]; 
        
        // 区间内：两点之间纯线性插值
        for (int i = 1; i < valid_cal_points - 1; i++) {
            if (current_delta >= Cal_Data_Delta[i] && current_delta <= Cal_Data_Delta[i+1]) {
                float frac = (float)(current_delta - Cal_Data_Delta[i]) / (float)(Cal_Data_Delta[i+1] - Cal_Data_Delta[i]);
                return (uint16_t)round(Sheet_Nodes[i] + frac * (Sheet_Nodes[i+1] - Sheet_Nodes[i]));
            }
        }
        
        // 区间外：最后两点纯线性外推
        int last = valid_cal_points - 1, prev = valid_cal_points - 2;
        float frac = (float)(current_delta - Cal_Data_Delta[prev]) / (float)(Cal_Data_Delta[last] - Cal_Data_Delta[prev]);
        return (uint16_t)round(Sheet_Nodes[prev] + frac * (Sheet_Nodes[last] - Sheet_Nodes[prev]));
        
    } else { 
        if (current_delta >= Cal_Data_Delta[1]) return Sheet_Nodes[1]; 
        
        for (int i = 1; i < valid_cal_points - 1; i++) {
            if (current_delta <= Cal_Data_Delta[i] && current_delta >= Cal_Data_Delta[i+1]) {
                float frac = (float)(Cal_Data_Delta[i] - current_delta) / (float)(Cal_Data_Delta[i] - Cal_Data_Delta[i+1]);
                return (uint16_t)round(Sheet_Nodes[i] + frac * (Sheet_Nodes[i+1] - Sheet_Nodes[i]));
            }
        }
        
        int last = valid_cal_points - 1, prev = valid_cal_points - 2;
        float frac = (float)(Cal_Data_Delta[prev] - current_delta) / (float)(Cal_Data_Delta[prev] - Cal_Data_Delta[last]);
        return (uint16_t)round(Sheet_Nodes[prev] + frac * (Sheet_Nodes[last] - Sheet_Nodes[prev]));
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  printf("\r\n=== Paper Counter V10.4 (Pure Linear Math) ===\r\n");
  OLED_Init();
  OLED_Clear();
  
  if (FDC2214_Init() == 0) {
      OLED_ShowString(1, 1, "Sensor OK!    ");
      Buzz_Beep(200);
  } else {
      OLED_ShowString(1, 1, "ERR: NO SENSOR");
      while(1) { Buzz_Beep(300); HAL_Delay(300); }
  }
  
  HAL_Delay(500);
  OLED_Clear();
  
  char display_buf[20]; 
  OLED_ShowString(1, 1, "Step 1: Cal    ");
  sprintf(display_buf, "Insert: %-2d sht ", Sheet_Nodes[cal_step]);
  OLED_ShowString(2, 1, display_buf);
  OLED_ShowString(3, 1, "R:0            "); 
  OLED_ShowString(4, 1, "Press CAL KEY  "); 
  
  uint32_t current_raw = 0;
  uint32_t last_ui_tick = 0; 

  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

      // ==========================================
      // 【后台任务】：实时刷新 Delta 数据 (仅未测量时)
      // ==========================================
      if (sys_state != STATE_MEASURED && (HAL_GetTick() - last_ui_tick > 150)) {
          last_ui_tick = HAL_GetTick();
          uint32_t live_raw = FDC2214_ReadCH0_Raw(); 

          if (live_raw == 0) {
              FDC2214_Init(); 
              OLED_ShowString(3, 1, "D: SENSOR RST "); 
          } else {
              char rt_buf[17];
              if (cal_step == 0) {
                  sprintf(rt_buf, "R:%-12lu", live_raw);
              } else {
                  long long live_delta = (long long)live_raw - (long long)Air_Base_Raw;
                  sprintf(rt_buf, "D:%-12lld", live_delta);
              }
              OLED_ShowString(3, 1, rt_buf); 
          }
      }

      // ==========================================
      // 【按键 1】：CAL 键 (校准收集按键)
      // ==========================================
      if (HAL_GPIO_ReadPin(KEY_CAL_GPIO_Port, KEY_CAL_Pin) == GPIO_PIN_RESET)
      {
          HAL_Delay(20);
          if (HAL_GPIO_ReadPin(KEY_CAL_GPIO_Port, KEY_CAL_Pin) == GPIO_PIN_RESET)
          {
              if (sys_state == STATE_CALIBRATING) 
              {
                  OLED_ShowString(4, 1, "Wait...       ");
                  HAL_Delay(800); 
                  
                  OLED_ShowString(4, 1, "Reading...    ");
                  current_raw = Get_Filtered_FDC_Data(); 
                  
                  if (cal_step == 0) {
                      Air_Base_Raw = current_raw;     
                      Cal_Data_Delta[0] = 0;
                      printf("[CAL 0 sht] Air Base: %lu, Delta: 0\r\n", Air_Base_Raw);
                  } else {
                      long long delta = (long long)current_raw - (long long)Air_Base_Raw;
                      Cal_Data_Delta[cal_step] = delta;
                  }
                  
                  Buzz_Beep(100);
                  cal_step++; 
                  
                  if (cal_step < CAL_POINTS) {
                      sprintf(display_buf, "Insert: %-2d sht ", Sheet_Nodes[cal_step]);
                      OLED_ShowString(2, 1, display_buf);
                      OLED_ShowString(4, 1, "Press CAL KEY  "); 
                  } else {
                      valid_cal_points = CAL_POINTS;
                      sys_state = STATE_READY;
                      OLED_Clear();
                      OLED_ShowString(1, 1, "Cal Finished! ");
                      OLED_ShowString(2, 1, "Ready to test.");
                      OLED_ShowString(4, 1, "Press MEASURE ");
                  }
              }
              while(HAL_GPIO_ReadPin(KEY_CAL_GPIO_Port, KEY_CAL_Pin) == GPIO_PIN_RESET);
          }
      }
            
      // ==========================================
      // 【按键 2】：MEASURE 键 (测试按键 / 撤销校准键)
      // ==========================================
      if (HAL_GPIO_ReadPin(KEY_MEASURE_GPIO_Port, KEY_MEASURE_Pin) == GPIO_PIN_RESET)
      {
          HAL_Delay(20);
          if (HAL_GPIO_ReadPin(KEY_MEASURE_GPIO_Port, KEY_MEASURE_Pin) == GPIO_PIN_RESET)
          {
              // --- 撤销逻辑 ---
              if (sys_state == STATE_CALIBRATING) {
                  if (cal_step > 0) {
                      cal_step--; 
                      OLED_ShowString(4, 1, "Undo 1 Step... ");
                      Buzz_Beep(50); HAL_Delay(50); Buzz_Beep(50); 
                      
                      sprintf(display_buf, "Insert: %-2d sht ", Sheet_Nodes[cal_step]);
                      OLED_ShowString(2, 1, display_buf);
                      HAL_Delay(500); 
                      OLED_ShowString(4, 1, "Press CAL KEY  ");
                  } else {
                      OLED_ShowString(4, 1, "Cannot Undo!   ");
                      Buzz_Beep(200);
                      HAL_Delay(500);
                      OLED_ShowString(4, 1, "Press CAL KEY  ");
                  }
              }
              
              // --- 测量逻辑 ---
              else if (sys_state == STATE_READY || sys_state == STATE_MEASURED) 
              {
                  OLED_Clear();
                  OLED_ShowString(2, 1, "  Testing...  ");
                  OLED_ShowString(3, 1, " Please Wait  ");
                  
                  HAL_Delay(800); 
                  
                  current_raw = Get_Filtered_FDC_Data();
                  
                  if(current_raw > SHORT_CIRCUIT_RAW_THRESHOLD) {
                      OLED_Clear();
                      OLED_ShowString(1, 1, "!! WARNING !! ");
                      OLED_ShowString(2, 1, "SHORT CIRCUIT!");
                      OLED_ShowString(4, 1, "Press MEASURE "); 
                      Buzz_Beep(1000); 
                      sys_state = STATE_MEASURED; 
                  } else {
                      long long current_delta = (long long)current_raw - (long long)Air_Base_Raw;
                      uint16_t result = Calculate_Sheets(current_delta);
                      
                      OLED_Clear();
                      OLED_ShowBigNum(16, result, 3); // 巨型字体居中显示
                      
                      Buzz_Beep(300);
                      sys_state = STATE_MEASURED; 
                  }
              }
              while(HAL_GPIO_ReadPin(KEY_MEASURE_GPIO_Port, KEY_MEASURE_Pin) == GPIO_PIN_RESET);
          }
      }

      // ==========================================
      // 【按键 3】：ZERO 键 (跳过校准 / 强制重归校准)
      // ==========================================
      if (HAL_GPIO_ReadPin(KEY_ZERO_GPIO_Port, KEY_ZERO_Pin) == GPIO_PIN_RESET)
      {
          HAL_Delay(20);
          if (HAL_GPIO_ReadPin(KEY_ZERO_GPIO_Port, KEY_ZERO_Pin) == GPIO_PIN_RESET)
          {
              if (sys_state == STATE_CALIBRATING) 
              {
                  if (cal_step >= 2) {
                      valid_cal_points = cal_step; 
                      sys_state = STATE_READY;     
                      
                      OLED_Clear();
                      OLED_ShowString(1, 1, "Cal Skipped!  ");
                      sprintf(display_buf, "Saved %-2d Pts  ", valid_cal_points);
                      OLED_ShowString(2, 1, display_buf);
                      OLED_ShowString(4, 1, "Press MEASURE ");
                      Buzz_Beep(100);
                  } else {
                      OLED_ShowString(4, 1, "Need >=2 Pts! ");
                      Buzz_Beep(50); HAL_Delay(50); Buzz_Beep(50);
                  }
              }
              else if (sys_state == STATE_MEASURED || sys_state == STATE_READY)
              {
                  sys_state = STATE_CALIBRATING;
                  cal_step = 0;
                  OLED_Clear();
                  OLED_ShowString(1, 1, "Step 1: Cal    ");
                  sprintf(display_buf, "Insert: %-2d sht ", Sheet_Nodes[cal_step]);
                  OLED_ShowString(2, 1, display_buf);
                  OLED_ShowString(3, 1, "R:0            "); 
                  OLED_ShowString(4, 1, "Press CAL KEY  "); 
                  Buzz_Beep(150);
              }
              while(HAL_GPIO_ReadPin(KEY_ZERO_GPIO_Port, KEY_ZERO_Pin) == GPIO_PIN_RESET);
          }
      }
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

void Buzz_Beep(uint16_t ms) {
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_SET);
    HAL_Delay(ms);
    HAL_GPIO_WritePin(BUZZER_GPIO_Port, BUZZER_Pin, GPIO_PIN_RESET);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  * where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
