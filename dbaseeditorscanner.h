#pragma once

//#include "dbaseformattoken.h"

#include <QChar>

namespace dBaseEditor {
namespace Internal {

class Scanner
{
public:
    Scanner(const QChar *text, const int length);
};

}  // namespace Internal
}  // namespace dBaseEditor
