namespace RepeatBoost.Settings;

public sealed class SettingsSingleInstance : IDisposable
{
    public const string DefaultMutexName =
        @"Local\RepeatBoost.Settings.SingleInstance.v1";

    private readonly Mutex _mutex;
    private bool _disposed;

    private SettingsSingleInstance(Mutex mutex)
    {
        _mutex = mutex;
    }

    public static SettingsSingleInstance? TryAcquire(
        string mutexName = DefaultMutexName)
    {
        var mutex = new Mutex(
            initiallyOwned: true,
            name: mutexName,
            createdNew: out var createdNew);

        if (!createdNew)
        {
            mutex.Dispose();
            return null;
        }

        return new SettingsSingleInstance(mutex);
    }

    public void Dispose()
    {
        if (_disposed)
        {
            return;
        }

        _disposed = true;
        try
        {
            _mutex.ReleaseMutex();
        }
        finally
        {
            _mutex.Dispose();
        }
    }
}
