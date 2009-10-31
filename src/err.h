#include <assert.h>

#define OK 0
#define EINVAL 1
#define EIO 2
#define EDB 3
#define ENOTFOUND 4
#define EFAULT 5
#define ENOMEM 6
#define ENOSPC 7
#define ECLOCK 8

#ifdef NDEBUG
# define WARN_IF(expr) do { if (expr) return -EINVAL; } while(0)
# define BUG_IF(expr) do { if (expr) return -EFAULT; } while(0)
#else
# define WARN_IF(expr) assert(!(expr))
# define BUG_IF(expr) assert(!(expr))
#endif

#define WARN() WARN_IF(1)
#define BUG() BUG_IF(1)
