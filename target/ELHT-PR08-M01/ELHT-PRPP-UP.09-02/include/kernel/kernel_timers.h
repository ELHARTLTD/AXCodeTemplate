/**
 * \file kernel/kernel_timers.h
 * \brief Таймеры ядра
 * \ingroup kernel_os_timer
 */

#pragma once

#include "kernel/kernel_base.h"
#include "timers.h"


namespace kernel
{
  /**
   * \addtogroup kernel_os_timer
   * \{
   */

  //! Интерфейс памяти таймера ядра
  struct TimerMemoryInterface
  {
    //! Ссылка на статическую память программного таймера FreeRTOS
    StaticTimer_t& timer_memory;
  };

  //! Хранилище памяти таймера ядра
  struct TimerMemory
  {
    //! Статическая память программного таймера FreeRTOS
    StaticTimer_t timer_storage;

    //! Интерфейс памяти таймера ядра
    TimerMemoryInterface interface;

    /**
     * \brief Конструктор хранилища памяти таймера ядра
     */
    constexpr TimerMemory()
    : timer_storage{},
      interface{
        .timer_memory {timer_storage}
      }
    {
    }
  };

  /**
   * \brief Программный таймер ядра
   * \details
   * Является оберткой над программными таймерами FreeRTOS, которая проверяет
   * аргументы и правильность указателей, чтобы ошибка в использовании таймера
   * не приводила к ошибке операционной системе.
   * 
   * Вызывает задаваемую пользователем Callback-функцию в системной задаче
   * таймеров FreeRTOS. Функция задается с помощью \ref etl::delegate.
   * 
   * Позволяет задавать время срабатывания в тиках процессора, но не менее 1 мс
   * (см. настройки \ref FreeRTOSConfig.h в реализации программы)
   */
  class Timer
  {
  private:
    TimerMemoryInterface& _memory; //!< Интерфейс памяти таймера
    TimerHandle_t _handler; //!< Обработчик программного таймера FreeRTOS

  public:
    //! Тип метода Callback'а по срабатыванию таймера
    using callback_t = etl::delegate<void(Timer&)>;

    /**
     * \brief Callback, вызываемый по срабатыванию таймера
     * \details
     * \attention В Callback-е запрещено использовать функции, переводящие
     * задачу в режим ожидания или остановки.
     */
    callback_t callback;


    /**
     * \brief Конструктор таймера ядра
     * \brief memory Интерфейс памяти таймера
     */
    constexpr Timer(TimerMemoryInterface& memory)
    : _memory{memory},
      _handler{nullptr}
    {
    }


    /**
     * \brief Проверяет создан ли объект таймера в операционной системе
     * \retval true  Объект таймера создан
     * \retval false Объект таймера не создан
     */
    bool isCreated() const { return _handler != nullptr; }

    /**
     * \brief Проверяет запущен ли таймер
     * \details
     * Вызывает функцию операционной системы xTimerIsTimerActive(), но проверяет
     * правильность аргументов.
     * 
     * \retval true  Таймер запущен
     * \retval false Таймер остановлен
     */
    bool isActive() const;

    /**
     * \brief Создает программный таймер в операционной системе и задает
     * первоначальные настройки
     * \param period Длительность таймера в тиках процессора (не менее 1 мс,
     * см. конфигурацию FreeRTOS)
     * \param autoreload Флаг автоматического перезапуска таймера после
     * срабатывания
     * \details
     * Метод должен быть вызван до использования любого другого метода таймера.
     * 
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerCreateStatic()
     * и устанавливает callback-таймеар
     * 
     * \retval true Если таймер успешно зарегистрирован (создан)
     * \retval false Если не удалось создать таймер
     */
    bool create(uint32_t period, bool autoreload);

    /**
     * \brief Запускает (перезапускает) таймер
     * \param timeout Время ожидания отправки команды в очередь системной
     * задачи таймеров операционной системы (в тиках процессора)
     * \details
     * Отправляет команду на запуск таймера в системную задачу таймеров.
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerStart(), но
     * проверяет правильность аргументов.
     * 
     * При обработке команды происходит следующее:
     * - Если таймер остановлен, запускает таймер
     * - Если таймер активен, начинает отсчет таймера заново
     * 
     * Запуск таймера произойдет при переходе в системную задачу таймеров
     * и обработке команды (что может произойти сразу при отправке команды
     * в очередь, и на момент выхода из метода таймер уже будет запущен).
     * 
     * \note Метод не должен использоваться в прерывании.
     * Для прерываний см. метод \ref startFromISR()
     * 
     * \retval true  Отправлена команда на запуск таймера
     * \retval false Не удалось отправить команду на запуск таймера (в очереди
     * системной задачи таймеров не появилось свободного места за время timeout)
     */
    bool start(uint32_t timeout);

    /**
     * \brief Запускает (перезапускает) таймер из прерывания
     * \param[out] pxHigherPriorityTaskWoken Опциональный флаг переключения
     * на более высокоприоритетную задачу при запуске таймера
     * \details
     * Отправляет команду на запуск таймера в системную задачу таймеров
     * из прерывания. Вызывает функцию операционной системы FreeRTOS
     * \ref xTimerStartFromISR(), но проверяет правильность аргументов.
     * 
     * При обработке команды происходит следующее:
     * - Если таймер остановлен, запускает таймер
     * - Если таймер активен, начинает отсчет таймера заново
     * 
     * Запуск таймера произойдет при переходе в системную задачу таймеров
     * и обработке команды (что будет не раньше выхода из прерывания).
     * 
     * Если аргумент pxHigherPriorityTaskWoken не является нулевым указателем,
     * метод запишет в него \ref pdTRUE в случае необходимости смены задачи
     * при выходе из прерывания. В таком случае см. раздел
     * \ref kernel_interrupts_section_contextSwitch документации на ядро.
     * 
     * \note Метод должен использоваться в прерывании.
     * Вне прерывания см. метод \ref start()
     * 
     * \retval true  Отправлена команда на запуск таймера
     * \retval false Не удалось отправить команду на запуск таймера (очередь
     * системной задачи таймеров занята)
     */
    bool startFromISR(BaseType_t* pxHigherPriorityTaskWoken);

    /**
     * \brief Останавливает таймер
     * \param timeout Время ожидания отправки команды в очередь системной
     * задачи таймеров операционной системы (в тиках процессора)
     * \details
     * Отправляет команду на остановку таймера в системную задачу таймеров.
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerStop(),
     * но проверяет правильность аргументов.
     * 
     * Остановка таймера произойдет при переходе в системную задачу таймеров
     * и обработке команды (что может произойти сразу при отправке команды
     * в очередь, и на момент выхода из метода таймер уже будет запущен).
     * 
     * \note Метод не должен использоваться в прерывании. Для прерываний 
     * см. метод \ref stopFromISR()
     * 
     * \retval true  Отправлена команда на остановку таймера
     * \retval false Не удалось отправить команду на остановку таймера
     * (в очереди системной задачи таймеров не появилось свободного места
     * за время timeout)
     */
    bool stop(uint32_t timeout);

    /**
     * \brief Останавливает таймер из прерывания
     * \param[out] pxHigherPriorityTaskWoken Опциональный флаг переключения
     * на более высокоприоритетную задачу при запуске таймера
     * \details
     * Отправляет команду на остановку таймера в системную задачу таймеров.
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerStopFromISR(),
     * но проверяет правильность аргументов.
     * 
     * \note Метод должен использоваться в прерывании.
     * Вне прерывания см. метод \ref stop()
     * 
     * Если аргумент pxHigherPriorityTaskWoken не является нулевым указателем,
     * метод запишет в него \ref pdTRUE в случае необходимости смены задачи
     * при выходе из прерывания. В таком случае см. раздел
     * \ref kernel_interrupts_section_contextSwitch документации на ядро.
     * 
     * \retval true  Отправлена команда на остановку таймера
     * \retval false Не удалось отправить команду на остановку таймера (очередь
     * системной задачи таймеров занята)
     */
    bool stopFromISR(BaseType_t* pxHigherPriorityTaskWoken);

    /**
     * \brief Перезапускает таймер с текущими настройками
     * \param timeout Время ожидания отправки команды в очередь системной
     * задачи таймеров операционной системы (в тиках процессора)
     * \details
     * Отправляет команду на перезапуск таймера в системную задачу таймеров.
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerReset(),
     * но проверяет правильность аргументов.
     * 
     * \note Метод не должен использоваться в прерывании. Для прерываний 
     * см. метод \ref resetFromISR()
     * 
     * Перезапуск таймера произойдет при переходе в системную задачу таймеров
     * и обработке команды (что может произойти сразу при отправке команды
     * в очередь, и на момент выхода из метода таймер уже будет перезапущен).
     * 
     * \retval true  Отправлена команда на перезапуск таймера
     * \retval false Не удалось отправить команду на перезапуск таймера
     */
    bool reset(uint32_t timeout);

    /**
     * \brief Перезапускает таймер из прерывания
     * \param[out] pxHigherPriorityTaskWoken Опциональный флаг переключения
     * на более высокоприоритетную задачу при запуске таймера
     * \details
     * Отправляет команду на перезапуск таймера в системную задачу таймеров.
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerResetFromISR(),
     * но проверяет правильность аргументов.
     * 
     * \note Метод должен использоваться в прерывании.
     * Вне прерывания см. метод \ref reset()
     * 
     * Если аргумент pxHigherPriorityTaskWoken не является нулевым указателем,
     * метод запишет в него \ref pdTRUE в случае необходимости смены задачи
     * при выходе из прерывания. В таком случае см. раздел
     * \ref kernel_interrupts_section_contextSwitch документации на ядро.
     * 
     * \retval true  Отправлена команда на перезапуск таймера
     * \retval false Не удалось отправить команду на перезапуск таймера
     * (очередь системной задачи таймеров занята)
     */
    bool resetFromISR(BaseType_t* pxHigherPriorityTaskWoken);

    /**
     * \brief Возвращает длительность (период) работы таймера в тиках
     * процессора
     * \details
     * Вызывает функцию операционной системы FreeRTOS \ref xTimerGetPeriod(),
     * но проверяет правильность аргументов
     */
    uint32_t getPeriod() const;

    /**
     * \brief Возвращает системное время в тиках процессора, в которое 
     * сработает запущенный таймер
     * \details
     * Вызывает функцию операционной системы FreeRTOS 
     * \ref xTimerGetExpiryTime(), но проверяет правильность аргументов.
     */
    uint32_t getExpiryTime() const;

    /**
     * \brief Возвращает флаг автоматической перезагрузки таймера
     * \details
     * Вызывает функцию операционной системы FreeRTOS
     * \ref xTimerGetReloadMode(), но проверяет правильность аргументов.
     * 
     * \retval true  Таймер перезапустится после срабатывания
     * \retval false Таймер не будет перезапущен после срабатывания
     */
    bool getAutoReload() const;

    /**
     * \brief Задает длительность (период) работы таймера в тиках процессора
     * \param period Длительность (период) работы таймера
     * \param timeout Время ожидания отправки команды в очередь системной
     * задачи таймеров операционной системы (в тиках процессора)
     * \details
     * Отправляет команду в системную задачу таймеров на обновление периода
     * работы таймера. Вызывает функцию перационной системы FreeRTOS
     * \ref xTimerChangePeriod(), но проверяет правильность аргументов.
     * 
     * \note Метод не должен использоваться в прерывании. Для прерываний 
     * см. метод \ref setPeriodFromISR()
     * 
     * Установка длительности работы таймера произойдет при переходе 
     * в системную задачу таймеров и обработке команды (что может произойти 
     * сразу при отправке команды в очередь, и на момент выхода из метода 
     * таймер уже будет запущен)
     * 
     * \retval true  Отправлена команда на установку периода таймера
     * \retval false Не удалось отправить команду на установку периода таймера
     * (в очереди системной задачи таймеров не появилось свободного места
     * за время timeout)
     */
    bool setPeriod(uint32_t period, uint32_t timeout);

    /**
     * \brief Задает длительность (период) работы таймера в тиках процессора
     * из прерывания
     * \param[in]  period Длительность (период) работы таймера
     * \param[out] pxHigherPriorityTaskWoken Опциональный флаг переключения
     * на более высокоприоритетную задачу при запуске таймера
     * \details
     * Отправляет команду в системную задачу таймеров на обновление периода
     * работы таймера. Вызывает функцию операционной системы FreeRTOS
     * \ref xTimerChangePeriodFromISR(), но проверяет правильность аргументов.
     * 
     * \note Метод должен использоваться в прерывании.
     * Вне прерывания см. метод \ref setPeriod()
     * 
     * Если аргумент pxHigherPriorityTaskWoken не является нулевым указателем,
     * метод запишет в него \ref pdTRUE в случае необходимости смены задачи
     * при выходе из прерывания. В таком случае см. раздел
     * \ref kernel_interrupts_section_contextSwitch документации на ядро.
     * 
     * \retval true  Отправлена команда на установку периода таймера
     * \retval false Не удалось отправить команду на установку периода таймера
     * (очередь системной задачи таймеров занята)
     */
    bool setPeriodFromISR(
      uint32_t period, BaseType_t* pxHigherPriorityTaskWoken
    );

    /**
     * \brief Устанавливает режим автоперезагрузки таймера
     * \param state Новое состояние автоперезагрузки таймера (true - включен)
     * \details
     * Вызывает функцию операционной системы FreeRTOS
     * \ref vTimerSetReloadMode(), но проверяет правильность аргументов.
    */
    void setAutoReload(bool state);
  };

  /**
   * \}
   * \noop kernel_os_timer
   */
}
