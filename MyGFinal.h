#ifndef MyGFinal_DEFINED
#define MyGFinal_DEFINED
#include "include/GFinal.h"
#include "include/GPath.h"
#include "include/GPathBuilder.h"
#include "include/GShader.h"

class MyGFinal : public GFinal {
public:
    MyGFinal();
    std::shared_ptr<GShader> createLinearPosGradient(GPoint p0, GPoint p1, const GColor colors[], const float pos[], int count) override;
    ~MyGFinal() override = default;
};

#endif








