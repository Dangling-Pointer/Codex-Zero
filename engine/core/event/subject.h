#pragma once
#include <vector>
#include <algorithm>

namespace elysia::core
{
template<typename ObserverType>
class Subject
{
public:
    Subject() = default;
    virtual ~Subject() = default;

    Subject(const Subject&) = delete;
    Subject& operator=(const Subject&) = delete;

    Subject(Subject&&) = delete;
    Subject& operator=(Subject&&) = delete;

    void attach(ObserverType* observer)
    {
        if (!observer)
            return;

        auto iter = std::find(_observers.begin(), _observers.end(), observer);
        if (iter != _observers.end())
            return;

        _observers.push_back(observer);
    }

    void detach(ObserverType* observer)
    {
        _observers.erase(
            std::remove(_observers.begin(), _observers.end(), observer),
            _observers.end()
        );
    }

protected:
    template<typename NotifyFunc>
    void notify_observers(NotifyFunc&& notify_func)
    {
        const auto observers_copy = _observers;

        for (ObserverType* observer : observers_copy)
        {
            if (observer)
                notify_func(*observer);
        }
    }

private:
    std::vector<ObserverType*> _observers;
};
}
