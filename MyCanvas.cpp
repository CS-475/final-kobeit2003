#include "include/GCanvas.h"
#include "include/GPaint.h"
#include "include/GRect.h"
#include "include/GPixel.h"
#include "include/GMatrix.h"
#include "include/GPath.h"
#include "include/GPathBuilder.h"
#include "MyCanvas.h"
#include "BlendFunctions.h"
#include "edgeUtils.h"
#include "GShader.h"


#include <iostream>






MyCanvas::MyCanvas(GBitmap& bitmap) : fBitmap(bitmap) {
}


MyCanvas::~MyCanvas() {
}


GPixel MyCanvas::ConvertColor(const GColor& color) const {
   uint8_t a = static_cast<uint8_t>(color.a * 255 + 0.5f);
   uint8_t r = static_cast<uint8_t>(color.r * color.a * 255 + 0.5f);
   uint8_t g = static_cast<uint8_t>(color.g * color.a * 255 + 0.5f);
   uint8_t b = static_cast<uint8_t>(color.b * color.a * 255 + 0.5f);
   return GPixel_PackARGB(a, r, g, b);
}


void MyCanvas::clear(const GColor& color) {
   int height = fBitmap.height();
   int width = fBitmap.width();
   GPixel p = ConvertColor(color);
   GPixel *row_addr = nullptr;
   for (int y = 0; y < height; y++) {
       row_addr = fBitmap.getAddr(0, y);
       for (int x = 0; x < width; x++) {
           row_addr[x] = p;
       }
   }
}




GPixel MyCanvas::blendPixel(GPixel dstPixel, GPixel srcPixel, GBlendMode blendMode) const {
   BlendProc blendFunc = findBlend(blendMode);
   return blendFunc(srcPixel, dstPixel);
}


void MyCanvas::drawRect(const GRect& rect, const GPaint& paint) {
   GPoint points[4] = {
       {rect.left, rect.top},    
       {rect.right, rect.top},   
       {rect.right, rect.bottom}, 
       {rect.left, rect.bottom}   
   };


   GPoint transformedPoints[4];
   for (int i = 0; i < 4; ++i) {
       float sx = points[i].x;
       float sy = points[i].y;
       transformedPoints[i].x = fMatrix[0] * sx + fMatrix[2] * sy + fMatrix[4];
       transformedPoints[i].y = fMatrix[1] * sx + fMatrix[3] * sy + fMatrix[5];
   }


   GIRect ir = GIRect::LTRB(
       static_cast<int32_t>(std::min({transformedPoints[0].x, transformedPoints[1].x, transformedPoints[2].x, transformedPoints[3].x})),
       static_cast<int32_t>(std::min({transformedPoints[0].y, transformedPoints[1].y, transformedPoints[2].y, transformedPoints[3].y})),
       static_cast<int32_t>(std::max({transformedPoints[0].x, transformedPoints[1].x, transformedPoints[2].x, transformedPoints[3].x})),
       static_cast<int32_t>(std::max({transformedPoints[0].y, transformedPoints[1].y, transformedPoints[2].y, transformedPoints[3].y}))
   );


   int left = std::max(0, ir.left);
   int top = std::max(0, ir.top);
   int right = std::min(fBitmap.width(), ir.right);
   int bottom = std::min(fBitmap.height(), ir.bottom);


   if (left >= right || top >= bottom) {
       return;
   }


   GShader* shader = paint.peekShader();
   GPixel srcPixel;
   bool useShader = (shader != nullptr && shader->setContext(fMatrix));


   for (int y = top; y < bottom; ++y) {
       GPixel* rowAddr = fBitmap.getAddr(0, y);
       for (int x = left; x < right; ++x) {
           if (useShader) {
               shader->shadeRow(x, y, 1, &srcPixel);
           } else {
               srcPixel = ConvertColor(paint.getColor());
           }
           if (paint.getBlendMode() == GBlendMode::kSrc) {
               rowAddr[x] = srcPixel;
           } else {
               rowAddr[x] = blendPixel(rowAddr[x], srcPixel, paint.getBlendMode());
           }
       }
   }
}




void MyCanvas::drawConvexPolygon(const GPoint points[], int count, const GPaint& paint) {
   GPoint transformedPoints[count];
   for (int i = 0; i < count; ++i) {
       float sx = points[i].x;
       float sy = points[i].y;
       transformedPoints[i].x = fMatrix[0] * sx + fMatrix[2] * sy + fMatrix[4];
       transformedPoints[i].y = fMatrix[1] * sx + fMatrix[3] * sy + fMatrix[5];
   }


   std::vector<Edge> edges;
   for (int i = 0; i < count; ++i) {
       int next = (i + 1) % count;
       Edge edge = makeEdge(transformedPoints[i], transformedPoints[next]);
       if (edge.isUseful()) {
           edges.push_back(edge);
       }
   }


   std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
       return a.top < b.top;
   });


   int canvasHeight = fBitmap.height();
   GShader* shader = paint.peekShader();
   bool useShader = (shader != nullptr && shader->setContext(fMatrix));
   GPixel srcPixel;


   for (int y = 0; y < canvasHeight; ++y) {
       std::vector<int> intersections;


       for (const auto& edge : edges) {
           if (y >= edge.top && y <= edge.bottom) {
               int x = static_cast<int>(std::round(edge.m * y + edge.b));
               intersections.push_back(x);
           }
       }


       std::sort(intersections.begin(), intersections.end());


       if (useShader) {
           shader->shadeRow(0, y, 1, &srcPixel);
       } else {
           srcPixel = ConvertColor(paint.getColor());
       }


       GPixel* rowAddr = fBitmap.getAddr(0, y);
       for (size_t i = 0; i < intersections.size(); i += 2) {
           if (i + 1 < intersections.size()) {
               int xStart = std::max(0, intersections[i]);
               int xEnd = std::min(fBitmap.width(), intersections[i + 1]);


               if (xStart < xEnd) {
                   for (int x = xStart; x < xEnd; ++x) {
                       if (useShader) {
                           shader->shadeRow(x, y, 1, &srcPixel);
                       }
                       rowAddr[x] = blendPixel(rowAddr[x], srcPixel, paint.getBlendMode());
                   }
               }
           }
       }
   }
}




void MyCanvas::save() {
   fMatrixStack.push(fMatrix);
}


void MyCanvas::restore() {
   if (!fMatrixStack.empty()) {
       fMatrix = fMatrixStack.top();
       fMatrixStack.pop();
   }
}


void MyCanvas::concat(const GMatrix& matrix) {
   fMatrix = GMatrix::Concat(fMatrix, matrix);
}


int computeQuadSegments(const GPoint pts[3], float tolerance) {
   float ax = pts[0].x - 2 * pts[1].x + pts[2].x;
   float ay = pts[0].y - 2 * pts[1].y + pts[2].y;
   float maxDist = std::sqrt(ax * ax + ay * ay);
   return static_cast<int>(std::ceil(std::sqrt(maxDist / tolerance)));


}


int computeCubicSegments(const GPoint pts[4], float tolerance) {
   float ax = -pts[0].x + 3 * (pts[1].x - pts[2].x) + pts[3].x;
   float ay = -pts[0].y + 3 * (pts[1].y - pts[2].y) + pts[3].y;
   float maxDist = std::sqrt(ax * ax + ay * ay);
   return static_cast<int>(std::ceil(std::sqrt(maxDist / tolerance)));
}


GPoint evalQuad(const GPoint pts[3], float t) {
   float u = 1 - t;
   return {
       u * u * pts[0].x + 2 * u * t * pts[1].x + t * t * pts[2].x,
       u * u * pts[0].y + 2 * u * t * pts[1].y + t * t * pts[2].y
   };
}


GPoint evalCubic(const GPoint pts[4], float t) {
   float u = 1 - t;
   return {
       u * u * u * pts[0].x + 3 * u * u * t * pts[1].x + 3 * u * t * t * pts[2].x + t * t * t * pts[3].x,
       u * u * u * pts[0].y + 3 * u * u * t * pts[1].y + 3 * u * t * t * pts[2].y + t * t * t * pts[3].y
   };
}


void MyCanvas::drawPath(const GPath& path, const GPaint& paint) {
   GPathBuilder builder;
   builder.reset();
   GPath::Iter iter(path);
   GPoint pts[GPath::kMaxNextPoints];
   const float tolerance = 0.25f;


   while (auto verb = iter.next(pts)) {
       switch (verb.value()) {
           case GPathVerb::kMove:
               builder.moveTo(pts[0]);
               break;
           case GPathVerb::kLine:
               builder.lineTo(pts[1]);
               break;
           case GPathVerb::kQuad: {
               int segments = computeQuadSegments(pts, tolerance);
               for (int i = 0; i < segments; ++i) {
                   float t1 = static_cast<float>(i + 1) / segments;
                   builder.lineTo(evalQuad(pts, t1));
               }
               break;
           }
           case GPathVerb::kCubic: {
               int segments = computeCubicSegments(pts, tolerance);
               for (int i = 0; i < segments; ++i) {
                   float t1 = static_cast<float>(i + 1) / segments;
                   builder.lineTo(evalCubic(pts, t1));
               }
               break;
           }
           default:
               break;
       }
   }


   builder.transform(fMatrix);
   auto transformedPath = builder.detach();
  
   GPath::Edger edger(*transformedPath);
   std::vector<Edge> edges;
   while (auto verb = edger.next(pts)) {
       if (verb.value() == GPathVerb::kLine) {
           Edge edge = makeEdge(pts[0], pts[1]);
           if (edge.isUseful()) {
               edges.push_back(edge);
           }
       }
   }
  
   std::sort(edges.begin(), edges.end(), [](const Edge& a, const Edge& b) {
       return a.top < b.top || (a.top == b.top && a.currentX < b.currentX);
   });


   int canvasHeight = fBitmap.height();
   int canvasWidth = fBitmap.width();
   GShader* shader = paint.peekShader();
   bool useShader = (shader != nullptr && shader->setContext(fMatrix));
   GPixel srcPixel;


   for (int y = 0; y < canvasHeight; ++y) {
       std::vector<int> xIntervals;
       int winding = 0;


       for (auto& edge : edges) {
           if (edge.isValid(y)) {
               int x = static_cast<int>(std::round(edge.computeX(y)));
               if (x < 0 || x >= canvasWidth) continue;
               xIntervals.push_back(x);
               winding += edge.wind;
           }
       }


       std::sort(xIntervals.begin(), xIntervals.end());


       for (size_t i = 0; i + 1 < xIntervals.size(); i += 2) {
           int left = xIntervals[i];
           int right = xIntervals[i + 1];
           left = std::max(0, left);
           right = std::min(canvasWidth, right);


           for (int xSpan = left; xSpan < right; ++xSpan) {
               if (useShader) {
                   shader->shadeRow(xSpan, y, 1, &srcPixel);
               } else {
                   srcPixel = ConvertColor(paint.getColor());
               }


               GPixel* dst = fBitmap.getAddr(xSpan, y);
               *dst = blendPixel(*dst, srcPixel, paint.getBlendMode());
           }
       }
   }
}




std::unique_ptr<GCanvas> GCreateCanvas(const GBitmap& bitmap) {
   return std::make_unique<MyCanvas>(const_cast<GBitmap&>(bitmap));
}




std::string GDrawSomething(GCanvas* canvas, GISize dim) {
  canvas->clear(GColor::RGBA(0.6f, 0.8f, 1.0f, 1.0f));




  float canvasCenterX = dim.width / 2.0f;
  float canvasCenterY = dim.height / 2.0f;




  GBitmap bitmap;
  GMatrix localMatrix;




  std::shared_ptr<GShader> shader = GCreateBitmapShader(bitmap, localMatrix);




  GPoint triangle[] = {
      {canvasCenterX - 50, canvasCenterY - 20},
      {canvasCenterX, canvasCenterY - 80},
      {canvasCenterX + 50, canvasCenterY - 20}
  };
  int triangleCount = sizeof(triangle) / sizeof(triangle[0]);
  GPaint greenPaint(GColor::RGBA(0.0f, 1.0f, 0.0f, 1.0f));
  greenPaint.setShader(shader);
  canvas->drawConvexPolygon(triangle, triangleCount, greenPaint);




  float octagonScale = 50.0f;
  GPoint octagon[] = {
      {canvasCenterX - octagonScale, canvasCenterY - 2 * octagonScale},
      {canvasCenterX + octagonScale, canvasCenterY - 2 * octagonScale},
      {canvasCenterX + 2 * octagonScale, canvasCenterY - octagonScale},
      {canvasCenterX + 2 * octagonScale, canvasCenterY + octagonScale},
      {canvasCenterX + octagonScale, canvasCenterY + 2 * octagonScale},
      {canvasCenterX - octagonScale, canvasCenterY + 2 * octagonScale},
      {canvasCenterX - 2 * octagonScale, canvasCenterY + octagonScale},
      {canvasCenterX - 2 * octagonScale, canvasCenterY - octagonScale}
  };
  int octagonCount = sizeof(octagon) / sizeof(octagon[0]);
  GPaint purplePaint(GColor::RGBA(0.5f, 0.0f, 0.5f, 1.0f));
  purplePaint.setShader(shader);
  canvas->drawConvexPolygon(octagon, octagonCount, purplePaint);




  GPoint trapezoid[] = {
      {canvasCenterX - 80, canvasCenterY + 110},
      {canvasCenterX + 70, canvasCenterY + 110},
      {canvasCenterX + 60, canvasCenterY + 170},
      {canvasCenterX - 60, canvasCenterY + 170}
  };
  int trapezoidCount = sizeof(trapezoid) / sizeof(trapezoid[0]);
  GPaint yellowPaint(GColor::RGBA(1.0f, 1.0f, 0.0f, 0.9f));
  yellowPaint.setShader(shader);
  canvas->drawConvexPolygon(trapezoid, trapezoidCount, yellowPaint);




  float pentagonScale = 0.8f;
  GPoint pentagon[] = {
      {canvasCenterX - 80 * pentagonScale, canvasCenterY - 60 * pentagonScale},
      {canvasCenterX, canvasCenterY - 90 * pentagonScale},
      {canvasCenterX + 70 * pentagonScale, canvasCenterY - 60 * pentagonScale},
      {canvasCenterX + 40 * pentagonScale, canvasCenterY + 50 * pentagonScale},
      {canvasCenterX - 50 * pentagonScale, canvasCenterY + 50 * pentagonScale}
  };
  int pentagonCount = sizeof(pentagon) / sizeof(pentagon[0]);
  GPaint pinkPaint(GColor::RGBA(1.0f, 0.7f, 0.8f, 1.0f));
  pinkPaint.setShader(shader);
  canvas->drawConvexPolygon(pentagon, pentagonCount, pinkPaint);




  return "Drawing Something.";
}


void MyCanvas::drawMesh(const GPoint verts[], const GColor colors[], const GPoint texs[],
                       int count, const int indices[], const GPaint& paint) {
    // Early exit checks
    if (!paint.peekShader()) {
        texs = nullptr;
    }
    if (!colors && !texs) {
        return;
    }

    // Process each triangle
    for (int i = 0; i < count; ++i) {
        // Get indices for current triangle
        int p0 = indices[3 * i];
        int p1 = indices[3 * i + 1];
        int p2 = indices[3 * i + 2];

        // Transform vertices using canvas matrix
        GPoint transformedVerts[3];
        GPoint originalVerts[3] = {verts[p0], verts[p1], verts[p2]};
        fMatrix.mapPoints(transformedVerts, originalVerts, 3);

        // Set up colors if present
        GColor triColors[3];
        if (colors) {
            triColors[0] = colors[p0];
            triColors[1] = colors[p1];
            triColors[2] = colors[p2];
        }

        // Set up texture coordinates if present
        GPoint triTexs[3];
        if (texs) {
            triTexs[0] = texs[p0];
            triTexs[1] = texs[p1];
            triTexs[2] = texs[p2];
        }

        drawTriangleInline(transformedVerts, 
                          colors ? triColors : nullptr,
                          texs ? triTexs : nullptr,
                          paint, fBitmap);
    }
}

void MyCanvas::drawQuad(const GPoint verts[4], const GColor colors[4], const GPoint texs[4],
                        int level, const GPaint& paint) {
    
    // Early exit checks
    if (level < 1 || !verts) {
        return;
    }
    if (!colors && !texs) {
        return;
    }

    try {
        
        std::vector<GPoint> generatedVerts;
        std::vector<GColor> generatedColors;
        std::vector<GPoint> generatedTexs;
        std::vector<int> indices;

        int numVerts = (level + 1) * (level + 1);

        // Generate vertices and interpolate colors/texture coordinates
        for (int i = 0; i <= level; ++i) {
            float v = static_cast<float>(i) / level;
            for (int j = 0; j <= level; ++j) {
                float u = static_cast<float>(j) / level;

                float x = verts[0].x * (1 - u) * (1 - v) +
                          verts[1].x * u * (1 - v) +
                          verts[3].x * (1 - u) * v +
                          verts[2].x * u * v;
                float y = verts[0].y * (1 - u) * (1 - v) +
                          verts[1].y * u * (1 - v) +
                          verts[3].y * (1 - u) * v +
                          verts[2].y * u * v;
                generatedVerts.push_back({x, y});

                if (colors) {
                    // Safer color interpolation
                    float c00 = (1 - u) * (1 - v);
                    float c10 = u * (1 - v);
                    float c01 = (1 - u) * v;
                    float c11 = u * v;
                    
                    GColor interpolatedColor;
                    interpolatedColor.a = c00 * colors[0].a + c10 * colors[1].a + c01 * colors[3].a + c11 * colors[2].a;
                    interpolatedColor.r = c00 * colors[0].r + c10 * colors[1].r + c01 * colors[3].r + c11 * colors[2].r;
                    interpolatedColor.g = c00 * colors[0].g + c10 * colors[1].g + c01 * colors[3].g + c11 * colors[2].g;
                    interpolatedColor.b = c00 * colors[0].b + c10 * colors[1].b + c01 * colors[3].b + c11 * colors[2].b;
                    generatedColors.push_back(interpolatedColor);
                }
                if (texs) {
                    float tx = (1 - u) * (1 - v) * texs[0].x +
                               u * (1 - v) * texs[1].x +
                               (1 - u) * v * texs[3].x +
                               u * v * texs[2].x;
                    float ty = (1 - u) * (1 - v) * texs[0].y +
                               u * (1 - v) * texs[1].y +
                               (1 - u) * v * texs[3].y +
                               u * v * texs[2].y;
                    generatedTexs.push_back({tx, ty});
                }
            }
        }

        // Generate indices
        for (int i = 0; i < level; ++i) {
            for (int j = 0; j < level; ++j) {
                int idx = i * (level + 1) + j;

                // Upper triangle
                indices.push_back(idx);
                indices.push_back(idx + 1);
                indices.push_back(idx + level + 1);

                // Lower triangle
                indices.push_back(idx + 1);
                indices.push_back(idx + level + 2);
                indices.push_back(idx + level + 1);
            }
        }

       
        drawMesh(generatedVerts.data(),
                colors ? generatedColors.data() : nullptr,
                texs ? generatedTexs.data() : nullptr,
                indices.size() / 3, indices.data(), paint);
        

    } catch (const std::exception& e) {    } catch (...) {
    }
}


void computeBarycentric(float x, float y, const GPoint& v0, const GPoint& v1, const GPoint& v2,
                        float& alpha, float& beta, float& gamma) {
    float denom = (v1.y - v2.y) * (v0.x - v2.x) + (v2.x - v1.x) * (v0.y - v2.y);
    alpha = ((v1.y - v2.y) * (x - v2.x) + (v2.x - v1.x) * (y - v2.y)) / denom;
    beta  = ((v2.y - v0.y) * (x - v2.x) + (v0.x - v2.x) * (y - v2.y)) / denom;
    gamma = 1.0f - alpha - beta;
}

GPixel colorToPixel(const GColor& color) {
    int a = static_cast<int>(color.a * 255 + 0.5f);
    int r = static_cast<int>(color.r * color.a * 255 + 0.5f);
    int g = static_cast<int>(color.g * color.a * 255 + 0.5f);
    int b = static_cast<int>(color.b * color.a * 255 + 0.5f);
    return GPixel_PackARGB(a, r, g, b);
}



void MyCanvas::drawTriangleInline(const GPoint verts[3], const GColor colors[3], const GPoint texs[3], 
                        const GPaint& paint, const GBitmap& device) {

    int left = std::max(0, static_cast<int>(std::floor(std::min({verts[0].x, verts[1].x, verts[2].x}))));
    int right = std::min(device.width(), static_cast<int>(std::ceil(std::max({verts[0].x, verts[1].x, verts[2].x}))));
    int top = std::max(0, static_cast<int>(std::floor(std::min({verts[0].y, verts[1].y, verts[2].y}))));
    int bottom = std::min(device.height(), static_cast<int>(std::ceil(std::max({verts[0].y, verts[1].y, verts[2].y}))));

    float dx_12 = verts[1].x - verts[0].x;
    float dy_12 = verts[1].y - verts[0].y;
    float dx_23 = verts[2].x - verts[1].x;
    float dy_23 = verts[2].y - verts[1].y;
    float dx_31 = verts[0].x - verts[2].x;
    float dy_31 = verts[0].y - verts[2].y;

    GShader* shader = paint.peekShader();
    bool useShader = shader && shader->setContext(GMatrix());

    float p_x = 0.5f;
    float p_y = 0.5f;

    for (int y = top; y < bottom; ++y) {
        GPixel* row = device.getAddr(0, y);
        float fy = y + p_y;

        for (int x = left; x < right; ++x) {
            float fx = x + p_x;

            float w0 = (dx_23 * (fy - verts[1].y) - dy_23 * (fx - verts[1].x));
            float w1 = (dx_31 * (fy - verts[2].y) - dy_31 * (fx - verts[2].x));
            float w2 = (dx_12 * (fy - verts[0].y) - dy_12 * (fx - verts[0].x));

            float area = dx_23 * dy_31 - dx_31 * dy_23;
            if (area == 0) continue;

            float invArea = 1.0f / area;
            w0 *= invArea;
            w1 *= invArea;
            w2 *= invArea;

            if (w0 >= 0 && w1 >= 0 && w2 >= 0) {
                GPixel srcPixel;

                if (useShader && texs) {
                    float tx = w0 * texs[0].x + w1 * texs[1].x + w2 * texs[2].x;
                    float ty = w0 * texs[0].y + w1 * texs[1].y + w2 * texs[2].y;
                    shader->shadeRow(tx, ty, 1, &srcPixel);
                } else if (colors) {
                    float a = w0 * colors[0].a + w1 * colors[1].a + w2 * colors[2].a;
                    float r = w0 * colors[0].r + w1 * colors[1].r + w2 * colors[2].r;
                    float g = w0 * colors[0].g + w1 * colors[1].g + w2 * colors[2].g;
                    float b = w0 * colors[0].b + w1 * colors[1].b + w2 * colors[2].b;

                    a = std::max(0.0f, std::min(1.0f, a));
                    r = std::max(0.0f, std::min(1.0f, r)) * a;
                    g = std::max(0.0f, std::min(1.0f, g)) * a;
                    b = std::max(0.0f, std::min(1.0f, b)) * a;

                    srcPixel = GPixel_PackARGB(
                        static_cast<uint8_t>(a * 255 + 0.5f),
                        static_cast<uint8_t>(r * 255 + 0.5f),
                        static_cast<uint8_t>(g * 255 + 0.5f),
                        static_cast<uint8_t>(b * 255 + 0.5f)
                    );
                }

                row[x] = blendPixel(row[x], srcPixel, paint.getBlendMode());
            }
        }
    }
}