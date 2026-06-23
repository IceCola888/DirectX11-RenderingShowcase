#include "Geometry.h"

namespace Geometry
{
    constexpr float PI = 3.1415926f;
    //
    // 幾何体メソッドの実装
    //
    
    GeometryData CreateSphere(float radius, uint32_t levels, uint32_t slices)
    {
        using namespace DirectX;

        GeometryData geoData;

        uint32_t vertexCount = 2 + (levels - 1) * (slices + 1);
        uint32_t indexCount = 6 * (levels - 1) * slices;
        geoData.vertices.resize(vertexCount);
        geoData.normals.resize(vertexCount);
        geoData.texcoords.resize(vertexCount);
        geoData.tangents.resize(vertexCount);
        if (indexCount > 65535)
            geoData.indices32.resize(indexCount);
        else
            geoData.indices16.resize(indexCount);

        uint32_t vIndex = 0, iIndex = 0;

        float phi = 0.0f, theta = 0.0f;
        float per_phi = PI / levels;
        float per_theta = 2 * PI / slices;
        float x, y, z;

        // 頂点を追加
        geoData.vertices[vIndex] = XMFLOAT3(0.0f, radius, 0.0f);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(0.0f, 0.0f);


        for (uint32_t i = 1; i < levels; ++i)
        {
            phi = per_phi * i;
            // slices + 1個の頂点が必要なのは、開始点と終了点が同じ位置であるが、テクスチャ座標が異なるため
            for (uint32_t j = 0; j <= slices; ++j)
            {
                theta = per_theta * j;
                x = radius * sinf(phi) * cosf(theta);
                y = radius * cosf(phi);
                z = radius * sinf(phi) * sinf(theta);
                // ローカル座標、法線ベクトル、接線ベクトル、テクスチャ座標を計算
                XMFLOAT3 pos = XMFLOAT3(x, y, z);

                geoData.vertices[vIndex] = pos;
                XMStoreFloat3(&geoData.normals[vIndex], XMVector3Normalize(XMLoadFloat3(&pos)));
                geoData.tangents[vIndex] = XMFLOAT4(-sinf(theta), 0.0f, cosf(theta), 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(theta / 2 / PI, phi / PI);
            }
        }

        // 底面の頂点を追加
        geoData.vertices[vIndex] = XMFLOAT3(0.0f, -radius, 0.0f);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, -1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(0.0f, 1.0f);


        // インデックスを追加
        if (levels > 1)
        {
            for (uint32_t j = 1; j <= slices; ++j)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = 0;
                    geoData.indices32[iIndex++] = j % (slices + 1) + 1;
                    geoData.indices32[iIndex++] = j;
                }
                else
                {
                    geoData.indices16[iIndex++] = 0;
                    geoData.indices16[iIndex++] = j % (slices + 1) + 1;
                    geoData.indices16[iIndex++] = j;
                }
            }
        }


        for (uint32_t i = 1; i < levels - 1; ++i)
        {
            for (uint32_t j = 1; j <= slices; ++j)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = (i - 1) * (slices + 1) + j;
                    geoData.indices32[iIndex++] = (i - 1) * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices32[iIndex++] = i * (slices + 1) + j % (slices + 1) + 1;

                    geoData.indices32[iIndex++] = i * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices32[iIndex++] = i * (slices + 1) + j;
                    geoData.indices32[iIndex++] = (i - 1) * (slices + 1) + j;
                }
                else
                {
                    geoData.indices16[iIndex++] = (i - 1) * (slices + 1) + j;
                    geoData.indices16[iIndex++] = (i - 1) * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices16[iIndex++] = i * (slices + 1) + j % (slices + 1) + 1;

                    geoData.indices16[iIndex++] = i * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices16[iIndex++] = i * (slices + 1) + j;
                    geoData.indices16[iIndex++] = (i - 1) * (slices + 1) + j;
                }

            }
        }

        // インデックスを順番に追加
        if (levels > 1)
        {
            for (uint32_t j = 1; j <= slices; ++j)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = (levels - 2) * (slices + 1) + j;
                    geoData.indices32[iIndex++] = (levels - 2) * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices32[iIndex++] = (levels - 1) * (slices + 1) + 1;
                }
                else
                {
                    geoData.indices16[iIndex++] = (levels - 2) * (slices + 1) + j;
                    geoData.indices16[iIndex++] = (levels - 2) * (slices + 1) + j % (slices + 1) + 1;
                    geoData.indices16[iIndex++] = (levels - 1) * (slices + 1) + 1;
                }
            }
        }

        return geoData;
    }

    GeometryData CreateBox(float width, float height, float depth)
    {
        using namespace DirectX;

        GeometryData geoData;

        geoData.vertices.resize(24);
        geoData.normals.resize(24);
        geoData.tangents.resize(24);
        geoData.texcoords.resize(24);

        float w2 = width / 2, h2 = height / 2, d2 = depth / 2;

        // 右面(+X面)
        geoData.vertices[0] = XMFLOAT3(w2, -h2, -d2);
        geoData.vertices[1] = XMFLOAT3(w2, h2, -d2);
        geoData.vertices[2] = XMFLOAT3(w2, h2, d2);
        geoData.vertices[3] = XMFLOAT3(w2, -h2, d2);
        // 左面(-X面)
        geoData.vertices[4] = XMFLOAT3(-w2, -h2, d2);
        geoData.vertices[5] = XMFLOAT3(-w2, h2, d2);
        geoData.vertices[6] = XMFLOAT3(-w2, h2, -d2);
        geoData.vertices[7] = XMFLOAT3(-w2, -h2, -d2);
        // 顶面(+Y面)
        geoData.vertices[8] = XMFLOAT3(-w2, h2, -d2);
        geoData.vertices[9] = XMFLOAT3(-w2, h2, d2);
        geoData.vertices[10] = XMFLOAT3(w2, h2, d2);
        geoData.vertices[11] = XMFLOAT3(w2, h2, -d2);
        // 底面(-Y面)
        geoData.vertices[12] = XMFLOAT3(w2, -h2, -d2);
        geoData.vertices[13] = XMFLOAT3(w2, -h2, d2);
        geoData.vertices[14] = XMFLOAT3(-w2, -h2, d2);
        geoData.vertices[15] = XMFLOAT3(-w2, -h2, -d2);
        // 背面(+Z面)
        geoData.vertices[16] = XMFLOAT3(w2, -h2, d2);
        geoData.vertices[17] = XMFLOAT3(w2, h2, d2);
        geoData.vertices[18] = XMFLOAT3(-w2, h2, d2);
        geoData.vertices[19] = XMFLOAT3(-w2, -h2, d2);
        // 正面(-Z面)
        geoData.vertices[20] = XMFLOAT3(-w2, -h2, -d2);
        geoData.vertices[21] = XMFLOAT3(-w2, h2, -d2);
        geoData.vertices[22] = XMFLOAT3(w2, h2, -d2);
        geoData.vertices[23] = XMFLOAT3(w2, -h2, -d2);

        for (size_t i = 0; i < 4; ++i)
        {
            // 右面(+X面)
            geoData.normals[i] = XMFLOAT3(1.0f, 0.0f, 0.0f);
            geoData.tangents[i] = XMFLOAT4(0.0f, 0.0f, 1.0f, 1.0f);
            // 左面(-X面)
            geoData.normals[i + 4] = XMFLOAT3(-1.0f, 0.0f, 0.0f);
            geoData.tangents[i + 4] = XMFLOAT4(0.0f, 0.0f, -1.0f, 1.0f);
            // 顶面(+Y面)
            geoData.normals[i + 8] = XMFLOAT3(0.0f, 1.0f, 0.0f);
            geoData.tangents[i + 8] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
            // 底面(-Y面)
            geoData.normals[i + 12] = XMFLOAT3(0.0f, -1.0f, 0.0f);
            geoData.tangents[i + 12] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
            // 背面(+Z面)
            geoData.normals[i + 16] = XMFLOAT3(0.0f, 0.0f, 1.0f);
            geoData.tangents[i + 16] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
            // 正面(-Z面)
            geoData.normals[i + 20] = XMFLOAT3(0.0f, 0.0f, -1.0f);
            geoData.tangents[i + 20] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        }

        for (size_t i = 0; i < 6; ++i)
        {
            geoData.texcoords[i * 4] = XMFLOAT2(0.0f, 1.0f);
            geoData.texcoords[i * 4 + 1] = XMFLOAT2(0.0f, 0.0f);
            geoData.texcoords[i * 4 + 2] = XMFLOAT2(1.0f, 0.0f);
            geoData.texcoords[i * 4 + 3] = XMFLOAT2(1.0f, 1.0f);
        }

        geoData.indices16.resize(36);

        uint16_t indices[] = {
            0, 1, 2, 2, 3, 0,		// 右面(+X面)
            4, 5, 6, 6, 7, 4,		// 左面(-X面)
            8, 9, 10, 10, 11, 8,	// 顶面(+Y面)
            12, 13, 14, 14, 15, 12,	// 底面(-Y面)
            16, 17, 18, 18, 19, 16, // 背面(+Z面)
            20, 21, 22, 22, 23, 20	// 正面(-Z面)
        };
        memcpy_s(geoData.indices16.data(), sizeof indices, indices, sizeof indices);

        return geoData;
    }

    GeometryData CreateCylinder(float radius, float height, uint32_t slices, uint32_t stacks, float texU, float texV)
    {
        using namespace DirectX;

        GeometryData geoData;
        uint32_t vertexCount = (slices + 1) * (stacks + 3) + 2;
        uint32_t indexCount = 6 * slices * (stacks + 1);

        geoData.vertices.resize(vertexCount);
        geoData.normals.resize(vertexCount);
        geoData.tangents.resize(vertexCount);
        geoData.texcoords.resize(vertexCount);

        if (indexCount > 65535)
            geoData.indices32.resize(indexCount);
        else
            geoData.indices16.resize(indexCount);

        float h2 = height / 2;
        float theta = 0.0f;
        float per_theta = 2 * PI / slices;
        float stackHeight = height / stacks;
        

        {
            // 下から上へ側面の頂点を配置する
            size_t vIndex = 0;
            for (size_t i = 0; i < stacks + 1; ++i)
            {
                float y = -h2 + i * stackHeight;
                // 現在の層の頂点
                for (size_t j = 0; j <= slices; ++j)
                {
                    theta = j * per_theta;
                    float u = theta / 2 / PI;
                    float v = 1.0f - (float)i / stacks;

                    geoData.vertices[vIndex] = XMFLOAT3(radius * cosf(theta), y, radius * sinf(theta)), XMFLOAT3(cosf(theta), 0.0f, sinf(theta));
                    geoData.normals[vIndex] = XMFLOAT3(cosf(theta), 0.0f, sinf(theta));
                    geoData.tangents[vIndex] = XMFLOAT4(-sinf(theta), 0.0f, cosf(theta), 1.0f);
                    geoData.texcoords[vIndex++] = XMFLOAT2(u * texU, v * texV);
                }
            }

            // インデックスを挿入する
            size_t iIndex = 0;
            for (uint32_t i = 0; i < stacks; ++i)
            {
                for (uint32_t j = 0; j < slices; ++j)
                {
                    if (indexCount > 65535)
                    {
                        geoData.indices32[iIndex++] = i * (slices + 1) + j;
                        geoData.indices32[iIndex++] = (i + 1) * (slices + 1) + j;
                        geoData.indices32[iIndex++] = (i + 1) * (slices + 1) + j + 1;

                        geoData.indices32[iIndex++] = i * (slices + 1) + j;
                        geoData.indices32[iIndex++] = (i + 1) * (slices + 1) + j + 1;
                        geoData.indices32[iIndex++] = i * (slices + 1) + j + 1;
                    }
                    else
                    {
                        geoData.indices16[iIndex++] = i * (slices + 1) + j;
                        geoData.indices16[iIndex++] = (i + 1) * (slices + 1) + j;
                        geoData.indices16[iIndex++] = (i + 1) * (slices + 1) + j + 1;

                        geoData.indices16[iIndex++] = i * (slices + 1) + j;
                        geoData.indices16[iIndex++] = (i + 1) * (slices + 1) + j + 1;
                        geoData.indices16[iIndex++] = i * (slices + 1) + j + 1;
                    }
                }
            }
        }

        //
        // 上蓋と下蓋の部分
        //
        {
            size_t vIndex = (slices + 1) * (stacks + 1), iIndex = 6 * slices * stacks;
            uint32_t offset = static_cast<uint32_t>(vIndex);

            // 上部の円の中心点を挿入
            geoData.vertices[vIndex] = XMFLOAT3(0.0f, h2, 0.0f);
            geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
            geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
            geoData.texcoords[vIndex++] = XMFLOAT2(0.5f, 0.5f);

            // 上部の円周上の各点を挿入
            for (uint32_t i = 0; i <= slices; ++i)
            {
                theta = i * per_theta;
                float u = cosf(theta) * radius / height + 0.5f;
                float v = sinf(theta) * radius / height + 0.5f;
                geoData.vertices[vIndex] = XMFLOAT3(radius * cosf(theta), h2, radius * sinf(theta));
                geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
                geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(u, v);
            }

            // 下部の円の中心点を挿入
            geoData.vertices[vIndex] = XMFLOAT3(0.0f, -h2, 0.0f);
            geoData.normals[vIndex] = XMFLOAT3(0.0f, -1.0f, 0.0f);
            geoData.tangents[vIndex] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
            geoData.texcoords[vIndex++] = XMFLOAT2(0.5f, 0.5f);

            // 下部の円周上の各点を挿入
            for (uint32_t i = 0; i <= slices; ++i)
            {
                theta = i * per_theta;
                float u = cosf(theta) * radius / height + 0.5f;
                float v = sinf(theta) * radius / height + 0.5f;
                geoData.vertices[vIndex] = XMFLOAT3(radius * cosf(theta), -h2, radius * sinf(theta));
                geoData.normals[vIndex] = XMFLOAT3(0.0f, -1.0f, 0.0f);
                geoData.tangents[vIndex] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(u, v);
            }


            // 上部の三角形のインデックスを挿入
            for (uint32_t i = 1; i <= slices; ++i)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = offset;
                    geoData.indices32[iIndex++] = offset + i % (slices + 1) + 1;
                    geoData.indices32[iIndex++] = offset + i;
                }
                else
                {
                    geoData.indices16[iIndex++] = offset;
                    geoData.indices16[iIndex++] = offset + i % (slices + 1) + 1;
                    geoData.indices16[iIndex++] = offset + i;
                }

            }

            // 下部の三角形のインデックスを挿入
            offset += slices + 2;
            for (uint32_t i = 1; i <= slices; ++i)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = offset;
                    geoData.indices32[iIndex++] = offset + i;
                    geoData.indices32[iIndex++] = offset + i % (slices + 1) + 1;
                }
                else
                {
                    geoData.indices16[iIndex++] = offset;
                    geoData.indices16[iIndex++] = offset + i;
                    geoData.indices16[iIndex++] = offset + i % (slices + 1) + 1;
                }
            }
        }


        return geoData;
    }

    GeometryData CreateCone(float radius, float height, uint32_t slices)
    {
        using namespace DirectX;

        GeometryData geoData;

        uint32_t vertexCount = 3 * slices + 1;
        uint32_t indexCount = 6 * slices;
        geoData.vertices.resize(vertexCount);
        geoData.normals.resize(vertexCount);
        geoData.tangents.resize(vertexCount);
        geoData.texcoords.resize(vertexCount);

        if (indexCount > 65535)
            geoData.indices32.resize(indexCount);
        else
            geoData.indices16.resize(indexCount);

        float h2 = height / 2;
        float theta = 0.0f;
        float per_theta = 2 * PI / slices;
        float len = sqrtf(height * height + radius * radius);

        //
        // 円錐の側面
        //
        {
            size_t iIndex = 0;
            size_t vIndex = 0;

            // 円錐の頂点を挿入（各頂点の位置は同じだが、異なる法線ベクトルと接線ベクトルを含む）
            for (uint32_t i = 0; i < slices; ++i)
            {
                theta = i * per_theta + per_theta / 2;
                geoData.vertices[vIndex] = XMFLOAT3(0.0f, h2, 0.0f);
                geoData.normals[vIndex] = XMFLOAT3(radius * cosf(theta) / len, height / len, radius * sinf(theta) / len);
                geoData.tangents[vIndex] = XMFLOAT4(-sinf(theta), 0.0f, cosf(theta), 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(0.5f, 0.5f);
            }

            // 円錐側面の底部の頂点を挿入
            for (uint32_t i = 0; i < slices; ++i)
            {
                theta = i * per_theta;
                geoData.vertices[vIndex] = XMFLOAT3(radius * cosf(theta), -h2, radius * sinf(theta));
                geoData.normals[vIndex] = XMFLOAT3(radius * cosf(theta) / len, height / len, radius * sinf(theta) / len);
                geoData.tangents[vIndex] = XMFLOAT4(-sinf(theta), 0.0f, cosf(theta), 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(cosf(theta) / 2 + 0.5f, sinf(theta) / 2 + 0.5f);
            }

            // インデックスを挿入
            for (uint32_t i = 0; i < slices; ++i)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = i;
                    geoData.indices32[iIndex++] = slices + (i + 1) % slices;
                    geoData.indices32[iIndex++] = slices + i % slices;
                }
                else
                {
                    geoData.indices16[iIndex++] = i;
                    geoData.indices16[iIndex++] = slices + (i + 1) % slices;
                    geoData.indices16[iIndex++] = slices + i % slices;
                }
            }
        }

        //
        // 円錐の底面
        //
        {
            size_t iIndex = 3 * (size_t)slices;
            size_t vIndex = 2 * (size_t)slices;

            // 円錐の底面の頂点を挿入
            for (uint32_t i = 0; i < slices; ++i)
            {
                theta = i * per_theta;

                geoData.vertices[vIndex] = XMFLOAT3(radius * cosf(theta), -h2, radius * sinf(theta)),
                    geoData.normals[vIndex] = XMFLOAT3(0.0f, -1.0f, 0.0f);
                geoData.tangents[vIndex] = XMFLOAT4(-1.0f, 0.0f, 0.0f, 1.0f);
                geoData.texcoords[vIndex++] = XMFLOAT2(cosf(theta) / 2 + 0.5f, sinf(theta) / 2 + 0.5f);
            }
            // 円錐の底面の中心点を挿入
            geoData.vertices[vIndex] = XMFLOAT3(0.0f, -h2, 0.0f),
                geoData.normals[vIndex] = XMFLOAT3(0.0f, -1.0f, 0.0f);
            geoData.texcoords[vIndex++] = XMFLOAT2(0.5f, 0.5f);

            // インデックスを挿入
            uint32_t offset = 2 * slices;
            for (uint32_t i = 0; i < slices; ++i)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = offset + slices;
                    geoData.indices32[iIndex++] = offset + i % slices;
                    geoData.indices32[iIndex++] = offset + (i + 1) % slices;
                }
                else
                {
                    geoData.indices16[iIndex++] = offset + slices;
                    geoData.indices16[iIndex++] = offset + i % slices;
                    geoData.indices16[iIndex++] = offset + (i + 1) % slices;
                }

            }
        }


        return geoData;
    }

    GeometryData CreatePlane(const DirectX::XMFLOAT2& planeSize, const DirectX::XMFLOAT2& maxTexCoord)
    {
        return CreatePlane(planeSize.x, planeSize.y, maxTexCoord.x, maxTexCoord.y);
    }

    GeometryData CreatePlane(float width, float depth, float texU, float texV)
    {
        using namespace DirectX;

        GeometryData geoData;

        geoData.vertices.resize(4);
        geoData.normals.resize(4);
        geoData.tangents.resize(4);
        geoData.texcoords.resize(4);


        uint32_t vIndex = 0;
        geoData.vertices[vIndex] = XMFLOAT3(-width / 2, 0.0f, -depth / 2);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(0.0f, texV);

        geoData.vertices[vIndex] = XMFLOAT3(-width / 2, 0.0f, depth / 2);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(0.0f, 0.0f);

        geoData.vertices[vIndex] = XMFLOAT3(width / 2, 0.0f, depth / 2);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(texU, 0.0f);

        geoData.vertices[vIndex] = XMFLOAT3(width / 2, 0.0f, -depth / 2);
        geoData.normals[vIndex] = XMFLOAT3(0.0f, 1.0f, 0.0f);
        geoData.tangents[vIndex] = XMFLOAT4(1.0f, 0.0f, 0.0f, 1.0f);
        geoData.texcoords[vIndex++] = XMFLOAT2(texU, texV);

        geoData.indices16 = { 0, 1, 2, 2, 3, 0 };

        return geoData;
    }

    GeometryData CreateGrid(const DirectX::XMFLOAT2& gridSize, const DirectX::XMUINT2& slices, const DirectX::XMFLOAT2& maxTexCoord,
        const std::function<float(float, float)>& heightFunc,
        const std::function<DirectX::XMFLOAT3(float, float)>& normalFunc,
        const std::function<DirectX::XMFLOAT4(float, float)>& colorFunc)
    {
        using namespace DirectX;

        GeometryData geoData;
        uint32_t vertexCount = (slices.x + 1) * (slices.y + 1);
        uint32_t indexCount = 6 * slices.x * slices.y;
        geoData.vertices.resize(vertexCount);
        geoData.normals.resize(vertexCount);
        geoData.tangents.resize(vertexCount);
        geoData.texcoords.resize(vertexCount);
        if (indexCount > 65535)
            geoData.indices32.resize(indexCount);
        else
            geoData.indices16.resize(indexCount);

        uint32_t vIndex = 0;
        uint32_t iIndex = 0;

        float sliceWidth = gridSize.x / slices.x;
        float sliceDepth = gridSize.y / slices.y;
        float leftBottomX = -gridSize.x / 2;
        float leftBottomZ = -gridSize.y / 2;
        float posX, posZ;
        float sliceTexWidth = maxTexCoord.x / slices.x;
        float sliceTexDepth = maxTexCoord.y / slices.y;

        XMFLOAT3 normal;
        XMFLOAT4 tangent;
        // メッシュの頂点を作成
        //  __ __
        // | /| /|
        // |/_|/_|
        // | /| /| 
        // |/_|/_|
        for (uint32_t z = 0; z <= slices.y; ++z)
        {
            posZ = leftBottomZ + z * sliceDepth;
            for (uint32_t x = 0; x <= slices.x; ++x)
            {
                posX = leftBottomX + x * sliceWidth;
                // 法線ベクトルを計算し、正規化
                normal = normalFunc(posX, posZ);
                XMStoreFloat3(&normal, XMVector3Normalize(XMLoadFloat3(&normal)));
                // 法線平面と z=posZ 平面が作る直線の単位接線ベクトルを計算し、w 成分を 1.0f に維持
                XMStoreFloat4(&tangent, XMVector3Normalize(XMVectorSet(normal.y, -normal.x, 0.0f, 0.0f)) + g_XMIdentityR3);

                geoData.vertices[vIndex] = XMFLOAT3(posX, heightFunc(posX, posZ), posZ);
                geoData.normals[vIndex] = normal;
                geoData.tangents[vIndex] = tangent;
                geoData.texcoords[vIndex++] = XMFLOAT2(x * sliceTexWidth, maxTexCoord.y - z * sliceTexDepth);
            }
        }
        // インデックスを挿入
        for (uint32_t i = 0; i < slices.y; ++i)
        {
            for (uint32_t j = 0; j < slices.x; ++j)
            {
                if (indexCount > 65535)
                {
                    geoData.indices32[iIndex++] = i * (slices.x + 1) + j;
                    geoData.indices32[iIndex++] = (i + 1) * (slices.x + 1) + j;
                    geoData.indices32[iIndex++] = (i + 1) * (slices.x + 1) + j + 1;

                    geoData.indices32[iIndex++] = (i + 1) * (slices.x + 1) + j + 1;
                    geoData.indices32[iIndex++] = i * (slices.x + 1) + j + 1;
                    geoData.indices32[iIndex++] = i * (slices.x + 1) + j;
                }
                else
                {
                    geoData.indices16[iIndex++] = i * (slices.x + 1) + j;
                    geoData.indices16[iIndex++] = (i + 1) * (slices.x + 1) + j;
                    geoData.indices16[iIndex++] = (i + 1) * (slices.x + 1) + j + 1;

                    geoData.indices16[iIndex++] = (i + 1) * (slices.x + 1) + j + 1;
                    geoData.indices16[iIndex++] = i * (slices.x + 1) + j + 1;
                    geoData.indices16[iIndex++] = i * (slices.x + 1) + j;
                }

            }
        }

        return geoData;
    }

}
