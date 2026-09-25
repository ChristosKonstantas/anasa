namespace anasa
{
    class IRenderKernel
    {
    public:
        IRenderKernel() = default;

        virtual float renderSample(int globalFrame, int version) const = 0;
    };
} // namespace anasa