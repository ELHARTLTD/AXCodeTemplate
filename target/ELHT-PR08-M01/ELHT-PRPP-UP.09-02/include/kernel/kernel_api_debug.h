/**
 * \file kernel/kernel_api_debug.h
 * \brief API отладки ядра
 * \ingroup kernel_api_debug
 * \details
 * Содержит классы и функции работы с отладочными сообщениями
 */

#pragma once

#include "kernel/kernel_base.h"
#include "etl/circular_buffer.h"
#include "menu.h"
#include "kernel/kernel_api_serial_menu.h"


/**
 * \brief Пространство имен отладки ядра
 * \ingroup kernel_api_debug
 */
namespace kernel::debug
{
  /**
   * \addtogroup kernel_api_debug
   * \{
   */

  //! Тип данных битовой маски типов отправляемых сообщений
  using print_type_t = uint16_t;

  //! Битовая маска типа отправляемого сообщения
  enum PRINT_TYPE : print_type_t
  {
    PRINT_TYPE_NONE  = 0x00, //!< Нет сообщения
    PRINT_TYPE_DEBUG = 0x01, //!< Сообщения отладки
    PRINT_TYPE_EVENT = 0x02, //!< Сообщения события
    PRINT_TYPE_ERROR = 0x04, //!< Сообщения ошибки
    PRINT_TYPE_ALL   = 0x07  //!< Все сообщения
  };


  /**
   * \brief Интерфейс буфера отладки
   * \details
   * Используется для записи отладочных сообщений и последующем их чтении
   * для отправки по интерфейсу связи или для использования для прочих задач.
   * 
   * Должен быть глобальной переменной (создаваться до запуска ядра
   * в глобальной области видимости)
   */
  class iDebugBuffer : public etl::forward_link<0>
  {
  public:
    //! Буфер сообщений для отправки
    etl::icircular_buffer<char>& buffer;
    //! Флаг разрешения записи в буфер отладки
    bool tx_permitted;
    //! Тип принимаемых в буфер сообщений
    print_type_t print_type;

    /**
     * \brief Конструктор интерфейса буфера отладки
     * \param buffer Буфер сообщений для отправки
     * \param print_type Тип принимаемых сообщений
     */
    iDebugBuffer(
      etl::icircular_buffer<char>& buffer,
      print_type_t print_type=PRINT_TYPE_ALL
    );

    //! Возвращает кол-во свободного места в буфере отладки
    size_t available();

    /**
     * \brief Копирует сообщение в буфер отладочных сообщений
     * \param tx_str Копируемая строка
     * \details
     * Записывает в буфер до N-5 байт, где N - кол-во байт в буфере сообщений
     * для отправки. Последние 5 байт буфера зарезервированы под отображение
     * переполнения буфера
     * \retval true Если посылка скопирована целиком
     * \retval false Если недостаточно места в буфере отправки
     */
    bool push(etl::string_view tx_str);
  };

  /**
   * \brief Буфер отладки
   * \tparam N Размер буфера отладки в байтах
   * \details
   * Используется для записи отладочных сообщений и последующем их чтении
   * для отправки по интерфейсу связи или для использования для прочих задач.
   * 
   * Должен быть глобальной переменной (создаваться до запуска ядра).
   */
  template<size_t N>
  class DebugBuffer : public iDebugBuffer
  {
    etl::circular_buffer<char, N> storage; //!< Буфер сообщений для отправки

  public:
    /**
     * \brief Конструктор буфера отладки
     * \param print_type Тип принимаемых в буфер сообщений
     */
    DebugBuffer(print_type_t print_type=PRINT_TYPE_ALL)
    : iDebugBuffer{storage, print_type},
      storage{}
    {
    }
  };


  /**
   * \brief Отправляет сообщение в буферы отладки
   * \param[in] from Отправляемая строка
   * \param[in] type Тип отправляемого сообщения (только 1 бит в маске)
   * \details
   * Сообщение будет отправлено во все буферы отладки \ref iDebugBuffer,
   * принимающие сообщения данного типа.
   * 
   * Для всех типов сообщений, кроме \ref PRINT_TYPE_DEBUG, в конец строки
   * автоматически добавляется "\r\n"
   */
  void print(const etl::string_view& from, PRINT_TYPE type);


  /**
   * \brief Форматирует и отправляет строку в буферы отладки, форматирование
   * происходит аналогично функции printf()
   * \tparam T Типы переменных для форматирования строки. Подставляются
   * компилятором автоматически. Кол-во совпадает с кол-вом аргументов args.
   * \param[in] from Форматируемая строка
   * \param[in] type Тип отправляемого сообщения (только 1 бит в маске)
   * \param[in] args Аргументы для форматирования строки. Кол-во аргументов 
   * должно совпадать с кол-вом спецификаторов формата в строке from
   * \details
   * Сообщение будет отправлено во все буферы отладки \ref iDebugBuffer,
   * принимающие сообщения данного типа.
   * 
   * Для всех типов сообщений, кроме \ref PRINT_TYPE_DEBUG, в конец строки
   * автоматически добавляется "\r\n"
   * 
   * Использует функцию snprintf() для исключения выхода за размер строки.  
   * Длина итоговой строки - не более 128 символов. При превышении вывод 
   * будет обрезан
   * 
   * \note Без специальных настроек компилятора не будет работать
   * форматирование float-переменных и переменных больше процессорного слова 
   * (например, 64-битные переменные)
   */
  template<typename... T>
  etl::enable_if_t<sizeof...(T), void> print(
    const etl::string_view& from, PRINT_TYPE type, T... args
  ){
    etl::array<char, 128> str;
    snprintf(str.data(), str.size(), from.data(), args...);
    print(str.data(), type);
  }


  /**
   * \brief Форматирует и отправляет отладочное сообщение в буферы отладки,
   * форматирование происходит аналогично функции printf()
   * \tparam T Типы переменных для форматирования строки. Подставляются
   * компилятором автоматически. Кол-во совпадает с кол-вом аргументов args.
   * \param[in] from Форматируемая строка отладочного сообщения
   * \param[in] args Аргументы для форматирования строки. Кол-во аргументов 
   * должно совпадать с кол-вом спецификаторов формата в строке from
   * \details
   * Краткая форма записи \ref print<T...>() для отправки отладочного сообщения
   * (\ref PRINT_TYPE_DEBUG)
   * 
   * \note 
   * 
   * \note Без специальных настроек компилятора не будет работать
   * форматирование float-переменных и переменных больше процессорного слова 
   * (например, 64-битные переменные)
   */
  template<typename... T>
  void print_debug(const etl::string_view& from, T... args)
  {
    return print(from, PRINT_TYPE_DEBUG, args...);
  }


  /**
   * \brief Форматирует и отправляет сообщение события в буферы отладки,
   * форматирование происходит аналогично функции printf()
   * \tparam T Типы переменных для форматирования строки. Подставляются
   * компилятором автоматически. Кол-во совпадает с кол-вом аргументов args.
   * \param[in] from Форматируемая строка сообщения события
   * \param[in] args Аргументы для форматирования строки. Кол-во аргументов 
   * должно совпадать с кол-вом спецификаторов формата в строке from
   * \details
   * Краткая форма записи \ref print<T...>() для отправки сообщения события
   * (\ref PRINT_TYPE_EVENT)
   * 
   * \note Без специальных настроек компилятора не будет работать
   * форматирование float-переменных и переменных больше процессорного слова 
   * (например, 64-битные переменные)
   */
  template<typename... T>
  void print_event(const etl::string_view& from, T... args)
  {
    return print(from, PRINT_TYPE_EVENT, args...);
  }


  /**
   * \brief Форматирует и отправляет сообщение ошиби в буферы отладки,
   * форматирование происходит аналогично функции printf()
   * \tparam T Типы переменных для форматирования строки. Подставляются
   * компилятором автоматически. Кол-во совпадает с кол-вом аргументов args.
   * \param[in] from Форматируемая строка сообщения ошиби
   * \param[in] args Аргументы для форматирования строки. Кол-во аргументов 
   * должно совпадать с кол-вом спецификаторов формата в строке from
   * \details
   * Краткая форма записи \ref print<T...>() для отправки сообщения ошибки
   * (\ref PRINT_TYPE_ERROR)
   * 
   * \note Без специальных настроек компилятора не будет работать
   * форматирование float-переменных и переменных больше процессорного слова 
   * (например, 64-битные переменные)
   */
  template<typename... T>
  void print_error(const etl::string_view& from, T... args)
  {
    return print(from, PRINT_TYPE_ERROR, args...);
  }


  /**
   * \brief Возвращает принятые пользовательские команды отладки
   * \param[out] to Строка, в которую будет скопирована пользовательская
   * команда отладки
   * \details
   * Копирует и удаляет из буфера пользовательских команд следующую строку
   * с командой.
   * 
   * В буфере может быть несколько пользовательских команд, строки разделяются
   * между собой символами '\\r', '\\n' или '\\0'.
   * 
   * Если в буфере нет пользовательских команд, строка to будет пустой (очищена)
   */
  void scan(etl::istring& to);


  //! Параметры таблицы настроек задачи отладки
  namespace parameter
  {
    /**
     * \brief Параметр "Тип отправляемых/принимаемых сообщений"
     * \ingroup kernel_api_debug
     * \details
     * Возможные значения параметра представляют собой битовую маску 
     * \ref PRINT_TYPE
     */
    inline const menu::var<print_type_t> print_type{{
      .default_value{PRINT_TYPE_ALL},
      .min_value{PRINT_TYPE_NONE},
      .max_value{PRINT_TYPE_ALL}
    }};
  }

  //! Список параметров таблицы настроек задачи отладки
  inline const menu::TableInfo debug_menu_info{
    serial::parameter::baudrate,
    serial::parameter::parity,
    serial::parameter::stop_bits,
    parameter::print_type
  };

  //! Тип таблицы параметров настроек задачи отладки
  using DebugMenu = decltype(menu::Table{debug_menu_info});

  /**
   * \}
   * \noop kernel_api_debug
   */
}


/**
 * \brief Замена встроенной функции printf() на отправку отладочных сообщений
 * \ingroup kernel_api_debug
 */
#define printf(format, ...) kernel::debug::print_debug(format, ##__VA_ARGS__ );