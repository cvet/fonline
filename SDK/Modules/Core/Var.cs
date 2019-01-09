using System;
using System.Linq;

namespace FOnlineEngine
{
    [AttributeUsage(AttributeTargets.Field)]
    public class ModifiableAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Field)]
    public class NonSerializableAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Field)]
    public class SerializeAsAttribute : Attribute
    {
        private string _varName;
        public string VarName { get { return _varName; } }

        public SerializeAsAttribute(string varName)
        {
            _varName = varName;
        }
    }

    [AttributeUsage(AttributeTargets.Field)]
    public class PublicSyncAttribute : Attribute
    {
    }

    [AttributeUsage(AttributeTargets.Field)]
    public class ProtectedSyncAttribute : Attribute
    {
    }

    public abstract class Var
    {
    }

    public sealed class Var<T> : Var
    {
        public delegate void ValueChangedDelegate(T value, T oldValue);
        public event ValueChangedDelegate ValueChangedEvent;
        public Func<T> CustomGetter;

        private Entity _owner;
        private Component _component;
        private string _name;
        private T _value;
        private bool _isProtected;
        private bool _isPublic;
        private bool _isModifiable;

#if SERVER
        private bool _isNotSerializable;
#endif

#if CLIENT
        private bool _isChosenVar;
#endif

        internal Var(Entity owner, Component component)
        {
            _owner = owner;
            _component = component;
            _name = GetType().Name;

#if CLIENT
            _isChosenVar = owner is Critter && owner.Id == InternalCalls.GetChosenId();
#endif

            foreach (var attr in GetType().GetCustomAttributes(true).Cast<Attribute>())
            {
                if (attr is PublicSyncAttribute)
                    _isPublic = true;
                else if (attr is ProtectedSyncAttribute)
                    _isProtected = true;
                else if (attr is SerializeAsAttribute)
                    _name = ((SerializeAsAttribute)attr).VarName;
                else if (attr is ModifiableAttribute)
                    _isModifiable = true;
#if SERVCER
                else if (attr is NonSerializableAttribute)
                    _isNotSerializable = true;
#endif
            }

            if (_component != null)
                _name = _component.GetType().Name + "_" + _name;
        }

        static public implicit operator T(Var<T> v)
        {
            return v.Value;
        }

        public T Value
        {
            get
            {
#if CLIENT
                if (!_isChosenVar && !_isPublic)
                    throw new InvalidOperationException();
                if (_isChosenVar && !_isPublic && !_isProtected)
                    throw new InvalidOperationException();
#endif

                if (CustomGetter != null)
                    return CustomGetter();

                return _value;
            }
            set
            {
#if CLIENT
                if (!_isChosenVar && (!_isPublic || !_isModifiable))
                    throw new InvalidOperationException();
                if (_isChosenVar && (!_isProtected || !_isModifiable))
                    throw new InvalidOperationException();
#endif

                T oldValue = _value;
                _value = value;

#if SERVER
                if (_isPublic)
                    InternalCalls.SendVariable(_owner.Id, _name, true, null);
                else if (_isProtected)
                    InternalCalls.SendVariable(_owner.Id, _name, false, null);

                if (!_isNotSerializable)
                    InternalCalls.SaveVariable(_owner.Id, _name, null);
#endif

#if CLIENT
                if (_isModifiable)
                    InternalCalls.SendModifiableVariable(_owner.Id, _name, null);
#endif

                if (ValueChangedEvent != null)
                {
                    foreach (var eventDelegate in ValueChangedEvent.GetInvocationList().Cast<ValueChangedDelegate>())
                    {
                        try
                        {
                            eventDelegate(value, oldValue);
                        }
                        catch (Exception ex)
                        {
                            Engine.LogError(ex);
                        }
                    }
                }
            }
        }
    }
}
