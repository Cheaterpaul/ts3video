#ifndef TCPPROTOCOL_HEADER
#define TCPPROTOCOL_HEADER

#include <stdint.h>

/*!
  COR-Protocol

  Base class for an correlated request.
  Each correlated request will have an associated response from server.

  Size: 
*/
typedef struct
{
  typedef uint16_t  version_t;
  typedef uint16_t  type_t;
  typedef uint8_t   flags_t;
  typedef uint32_t  correlation_t;
  typedef uint32_t  data_length_t;

  static const size_t MINSIZE = 16 + 16 + 8 + 32 + 32;
  static const type_t TYPE_REQUEST = 0;
  static const type_t TYPE_RESPONSE = 1;

  version_t version;             ///< (16-bit) The version number of the protocol.
  type_t type;                   ///< (16-bit) The type of the request.
  flags_t flags;                 ///< (8-bit)  Custom flags (Can be different, based on the type.)
  correlation_t correlation_id;  ///< (32-bit) Correlation ID which associates an request and response.
  data_length_t length;          ///< (32-bit) Length of the upcoming data block.
  uint8_t *data;
} cor_request_t;


/*!
*/
/*class ModuleRequest
{
public:
  typedef uint16_t name_size_t;
  typedef uint8_t  name_t;

  static const Request::type_t TYPE = 2;
  static const size_t SIZE = Request::SIZE + sizeof(name_size_t) + sizeof(name_size_t); // + value of "nameSize" and "actionSize"

  ModuleRequest() : base(0), nameSize(0), name(0), actionSize(0), action(0) {}
  ~ModuleRequest() { delete base; }

  Request *base;
  name_size_t nameSize;
  name_t *name;
  name_size_t actionSize;
  name_t *action;
};*/

#endif