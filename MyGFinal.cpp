
#include <cmath>
#include "MyGFinal.h"

MyGFinal::MyGFinal() : GFinal(){}
std::shared_ptr<GShader> MyGFinal::createLinearPosGradient(GPoint p0, GPoint p1, const GColor colors[], const float pos[], int count) {
    if (count < 1 || !colors || !pos) {
        return nullptr;
    }
    std::vector<GColor> interpolatedColors;
    for (float t = 0; t <= 1.0f; t += 0.01f) {  
        int index = 0;
        while (index < count - 1 && t > pos[index + 1]) {
            index++;
        }
        GColor color;
        if (t <= pos[0]) {
            color = colors[0];
        } else if (t >= pos[count - 1]) {
            color = colors[count - 1];
        } else {
            float time0 = pos[index];
            float time1 = pos[index + 1];
            float totalweight = (t - time0) / (time1 - time0);


            const GColor& abc0 = colors[index];
            const GColor& abc1 = colors[index + 1];
            color = {
                abc0.r + (abc1.r - abc0.r) * totalweight,
                abc0.g + (abc1.g - abc0.g) * totalweight,
                abc0.b + (abc1.b - abc0.b) * totalweight,
                abc0.a + (abc1.a - abc0.a) * totalweight
            };
        }
        interpolatedColors.push_back(color);
    }
    return GCreateLinearGradient(p0, p1, interpolatedColors.data(), interpolatedColors.size(), GTileMode::kClamp);
}

std::unique_ptr<GFinal> GCreateFinal() {
    return std::make_unique<MyGFinal>();
}
