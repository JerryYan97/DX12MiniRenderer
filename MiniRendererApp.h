class DX12MiniRenderer
{
public:
    DX12MiniRenderer();
    ~DX12MiniRenderer();

    /*
    * Create UIManager.
    * Create DX12 Device.
    */
    void Init();

    /*
    * The main loop of the application.
    */
    void Run();

    /*
    * Free all resources.
    */
    void Finalize();

private:

};