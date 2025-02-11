/**
 * \file 04_get_date.cpp
 * \brief Пример получения текущей даты и времени
 * \details
 * Получение значения часов реального времени с помощью объекта RTC.
 * В терминал выводятся текущие дата и время в формате:
 * Час:Мин:Сек День.Месяц.Год 
 * 
 * 
 * Для запуска примера скопируйте код в src/pou1.cpp.
 * Настройки POU в функции PLC_MainSetup() в файле pou_manager.cpp:
 * AddPOU(POU1, 1000, 1000);  // Период вызова 1000 мс, сторожевой таймер 1 сек
 * 
 */

#include "pou_manager.h"

using namespace plc;


void POU1()
{
  // Вывод текущих даты и времени в терминал
  print_debug(RTC.getTimeString());
  print_debug("\r\n");
}
