#pragma once
#include <Windows.h>
#include <memory>
#include <vector>
namespace fx3
{
    namespace DXRT
    {
        class DXRayTracerPSObject
        {
        private:
            /* data */
        public:
            DXRayTracerPSObject(/* args */);
            ~DXRayTracerPSObject();
            volatile void raytaracePSO();

        };
        
    } // namespace DXRT
    namespace DX12
    {
        template <class T>
        class PiplineStateObject final
        {
        private:
            /* data */
            std::unique_ptr<HMODULE> dlls_[];
            std::vector<T> objs[];
        public:
            PiplineStateObject(/* args */);
            ~PiplineStateObject();
        };
    } // namespace DX12
    
    
} // namespace fx3
