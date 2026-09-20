namespace FOnline;

#if SERVER

// The Game singleton bucket lock as a scope: `using GameLock scope = GameLock.Acquire();` takes the lock, and the
// end of the enclosing block releases it on every path out, an exception included.
//
// It is a ref struct for what the compiler then proves. The lock belongs to the thread that took it, and
// UnlockSingleton throws for any other; a continuation after an await may resume on a different thread, so the
// lock must never be held across one. A ref struct local cannot survive an await or a yield (CS4007), so the build
// rejects exactly that shape, in nested blocks and iterators too, without an analysis of its own.
//
// The raw Game.Lock / Game.Unlock pair is reserved for this type; FOSYNC005 reports it anywhere else
public readonly ref struct GameLock
{
    public static GameLock Acquire()
    {
        Game.Lock();
        return new GameLock();
    }

    // A copied scope disposed twice, or a default one, releases a lock this scope did not take; when nothing is
    // left held the engine throws, so the mistake is not silent
    public void Dispose()
    {
        Game.Unlock();
    }
}

#endif
