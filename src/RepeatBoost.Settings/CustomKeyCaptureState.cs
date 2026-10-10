namespace RepeatBoost.Settings;

internal struct CustomKeyCaptureState
{
    public bool IsActive { get; private set; }

    public void Start() => IsActive = true;

    public void Cancel() => IsActive = false;

    public bool TryAccept(int virtualKey)
    {
        if (!IsActive ||
            !CustomTargetSet.IsSelectableVirtualKey(virtualKey))
        {
            return false;
        }

        IsActive = false;
        return true;
    }
}
