//go:build tinygo
// +build tinygo

// time.go - Time functions for MyOS Go shell
// Uses kernel timer syscalls for timekeeping.

package runtime

import (
	"unsafe"
)

// Syscall numbers
const (
	SYS_TIME   = 23
	SYS_UPTIME = 27
	SYS_SLEEP  = 10
	SYS_YIELD  = 11
)

// Timer frequency (kernel timer runs at 1000 Hz = 1ms ticks)
const TimerFrequency = 1000

// Ticks per second
const TicksPerSecond = 1000

// Nanoseconds per tick
const NanosPerTick = 1_000_000 // 1ms = 1,000,000 ns

//go:linkname syscall Syscall
//go:noescape
func syscall(num, a1, a2, a3, a4, a5, a6 uintptr) (r1, r2 uintptr)

//go:noescape
func asm_hlt()

// =============================================================================
// Time Types (compatible with time package)
// =============================================================================

// Duration represents a time duration in nanoseconds
type Duration int64

// Time represents an instant in time
type Time struct {
	sec  int64 // seconds since epoch
	nsec int32 // nanosecond offset within second
}

// Common durations
const (
	Nanosecond  Duration = 1
	Microsecond Duration = 1000 * Nanosecond
	Millisecond Duration = 1000 * Microsecond
	Second      Duration = 1000 * Millisecond
	Minute      Duration = 60 * Second
	Hour        Duration = 60 * Minute
)

// =============================================================================
// Wall Clock Time
// =============================================================================

// Now returns the current wall-clock time
func Now() Time {
	sec := TimeNow()
	return Unix(sec, 0)
}

// Unix returns a Time from Unix seconds and nanoseconds
func Unix(sec int64, nsec int64) Time {
	if nsec < 0 || nsec >= 1_000_000_000 {
		// Normalize
		sec += nsec / 1_000_000_000
		nsec = nsec % 1_000_000_000
		if nsec < 0 {
			nsec += 1_000_000_000
			sec--
		}
	}
	return Time{sec: sec, nsec: int32(nsec)}
}

// TimeNow returns the current time in seconds since epoch
// Uses SYS_TIME syscall
func TimeNow() int64 {
	r1, _ := syscall(SYS_TIME, 0, 0, 0, 0, 0, 0, 0)
	return int64(r1)
}

// Uptime returns the system uptime in seconds
func Uptime() int64 {
	r1, _ := syscall(SYS_UPTIME, 0, 0, 0, 0, 0, 0, 0)
	return int64(r1)
}

// UptimeNanos returns the system uptime in nanoseconds
func UptimeNanos() int64 {
	// Get uptime in seconds and convert
	sec := Uptime()
	return sec * 1_000_000_000
}

// =============================================================================
// Monotonic Time (for timeouts, intervals)
// =============================================================================

// nanotime returns the current monotonic time in nanoseconds
// This uses the kernel's timer ticks
func nanotime() int64 {
	// Use uptime as monotonic clock
	return UptimeNanos()
}

// Ticks returns the current timer tick count
func Ticks() uint64 {
	return uint64(Uptime() * TicksPerSecond)
}

// =============================================================================
// Sleep Functions
// =============================================================================

// Sleep pauses the current goroutine for at least the duration d.
// Uses SYS_SLEEP syscall (milliseconds).
func Sleep(d Duration) {
	if d <= 0 {
		return
	}
	ms := int64(d / Millisecond)
	if ms == 0 {
		ms = 1 // Minimum 1ms
	}
	syscall(SYS_SLEEP, uintptr(ms), 0, 0, 0, 0, 0, 0)
}

// SleepMS sleeps for the given milliseconds
func SleepMS(ms int) {
	if ms <= 0 {
		return
	}
	syscall(SYS_SLEEP, uintptr(ms), 0, 0, 0, 0, 0, 0)
}

// SleepUS sleeps for the given microseconds (busy wait)
func SleepUS(us int) {
	if us <= 0 {
		return
	}
	// Busy wait using timer ticks
	start := nanotime()
	target := start + int64(us)*1000
	for nanotime() < target {
		// Spin
	}
}

// Yield yields the CPU to other processes
func Yield() {
	syscall(SYS_YIELD, 0, 0, 0, 0, 0, 0, 0)
}

// =============================================================================
// Time Arithmetic
// =============================================================================

// Add returns t + d
func (t Time) Add(d Duration) Time {
	nsec := int64(t.nsec) + int64(d)
	sec := t.sec + nsec/1_000_000_000
	nsec = nsec % 1_000_000_000
	if nsec < 0 {
		nsec += 1_000_000_000
		sec--
	}
	return Time{sec: sec, nsec: int32(nsec)}
}

// Sub returns the duration t - u
func (t Time) Sub(u Time) Duration {
	sec := t.sec - u.sec
	nsec := int64(t.nsec) - int64(u.nsec)
	return Duration(sec*1_000_000_000 + nsec)
}

// Before reports whether t is before u
func (t Time) Before(u Time) bool {
	if t.sec != u.sec {
		return t.sec < u.sec
	}
	return t.nsec < u.nsec
}

// After reports whether t is after u
func (t Time) After(u Time) bool {
	return u.Before(t)
}

// Equal reports whether t and u are equal
func (t Time) Equal(u Time) bool {
	return t.sec == u.sec && t.nsec == u.nsec
}

// =============================================================================
// Format/Parse (minimal)
// =============================================================================

// Format formats the time according to the format string (minimal)
func (t Time) Format(layout string) string {
	// Minimal implementation - just return a simple string
	hour := (t.sec / 3600) % 24
	min := (t.sec / 60) % 60
	sec := t.sec % 60
	return itoa2(int(hour)) + ":" + itoa2(int(min)) + ":" + itoa2(int(sec))
}

// itoa2 formats an integer as 2 digits
func itoa2(n int) string {
	if n < 10 {
		return "0" + itoa(n)
	}
	if n < 100 {
		return itoa(n)
	}
	return "99"
}

// itoa converts an integer to a string
func itoa(n int) string {
	if n == 0 {
		return "0"
	}
	buf := make([]byte, 20)
	i := len(buf)
	neg := n < 0
	if neg {
		n = -n
	}
	for n > 0 {
		i--
		buf[i] = byte('0' + n%10)
		n /= 10
	}
	if neg {
		i--
		buf[i] = '-'
	}
	return string(buf[i:])
}

// =============================================================================
// Timer Support (for runtime timers)
// =============================================================================

// Timer represents a single timer
type Timer struct {
	when     int64    // target time in nanoseconds
	period   int64    // period in nanoseconds (0 for one-shot)
	f        func()   // callback function
	active   bool
	index    int      // heap index
}

// Timer heap for runtime timers
var timers []*Timer

// startTimer adds a timer to the heap
func startTimer(t *Timer) {
	t.active = true
	t.index = len(timers)
	timers = append(timers, t)
	siftupTimer(len(timers) - 1)
}

// stopTimer removes a timer from the heap
func stopTimer(t *Timer) bool {
	if !t.active {
		return false
	}
	i := t.index
	last := len(timers) - 1
	if i != last {
		timers[i], timers[last] = timers[last], timers[i]
		timers[i].index = i
	}
	timers[last] = nil
	timers = timers[:last]
	if i < len(timers) {
		siftdownTimer(i)
		siftupTimer(i)
	}
	t.active = false
	return true
}

// siftupTimer maintains heap property upwards
func siftupTimer(i int) {
	for i > 0 {
		p := (i - 1) / 2
		if timers[p].when <= timers[i].when {
			break
		}
		timers[p], timers[i] = timers[i], timers[p]
		timers[p].index = p
		timers[i].index = i
		i = p
	}
}

// siftdownTimer maintains heap property downwards
func siftdownTimer(i int) {
	n := len(timers)
	for {
		l := 2*i + 1
		if l >= n {
			break
		}
		r := l + 1
		smallest := l
		if r < n && timers[r].when < timers[l].when {
			smallest = r
		}
		if timers[i].when <= timers[smallest].when {
			break
		}
		timers[i], timers[smallest] = timers[smallest], timers[i]
		timers[i].index = i
		timers[smallest].index = smallest
		i = smallest
	}
}

// checkTimers checks for expired timers and runs them
// Called from the scheduler/main loop
func CheckTimers() {
	now := nanotime()
	for len(timers) > 0 {
		t := timers[0]
		if t.when > now {
			break
		}
		// Pop timer
		stopTimer(t)
		// Run callback
		if t.f != nil {
			t.f()
		}
		// Re-arm if periodic
		if t.period > 0 {
			t.when = now + t.period
			startTimer(t)
		}
	}
}

// nextTimer returns the time until the next timer expires
func nextTimer() int64 {
	if len(timers) == 0 {
		return -1
	}
	when := timers[0].when
	now := nanotime()
	if when <= now {
		return 0
	}
	return when - now
}

// AfterFunc runs f after duration d. Returns a Timer that can be stopped.
func AfterFunc(d Duration, f func()) *Timer {
	t := &Timer{
		when:   nanotime() + int64(d),
		f:      f,
	}
	startTimer(t)
	return t
}

// Stop stops a timer. Returns true if the timer was stopped before firing.
func (t *Timer) Stop() bool {
	return stopTimer(t)
}

// Reset resets a timer to fire after duration d. Returns true if the timer was reset.
func (t *Timer) Reset(d Duration) bool {
	active := t.active
	stopTimer(t)
	t.when = nanotime() + int64(d)
	startTimer(t)
	return active
}

// =============================================================================
// Ticker (periodic timer)
// =============================================================================

// Ticker holds a channel that delivers ticks at intervals
type Ticker struct {
	C chan Time
	t *Timer
}

// NewTicker returns a new Ticker that ticks every duration d
func NewTicker(d Duration) *Ticker {
	c := make(chan Time, 1)
	t := &Timer{
		when:   nanotime() + int64(d),
		period: int64(d),
		f: func() {
			select {
			case c <- Now():
			default:
			}
		},
	}
	startTimer(t)
	return &Ticker{C: c, t: t}
}

// Stop stops the ticker
func (t *Ticker) Stop() {
	t.t.Stop()
	close(t.C)
}

// =============================================================================
// Time Components
// =============================================================================

// Date returns the year, month, day, hour, min, sec, nsec of the time
func (t Time) Date() (year, month, day int, hour, min, sec int, nsec int) {
	// Minimal implementation - return zeros
	return 1970, 1, 1, 0, 0, 0, int(t.nsec)
}

// Zone returns the time zone offset and name
func (t Time) Zone() (name string, offset int) {
	return "UTC", 0
}

// Location returns the time zone location
func (t Time) Location() *Location {
	return utcLoc
}

// Location represents a time zone
type Location struct {
	name string
	zone []zoneTrans
}

// zoneTrans represents a time zone transition
type zoneTrans struct {
	when int64
	index int
}

var utcLoc = &Location{name: "UTC"}

// UTC returns t in UTC
func (t Time) UTC() Time {
	return t
}

// Local returns t in local time
func (t Time) Local() Time {
	return t
}

// In returns t in the given location
func (t Time) In(loc *Location) Time {
	return t
}

// Round rounds the time to the nearest multiple of d
func (t Time) Round(d Duration) Time {
	if d <= 0 {
		return t
	}
	nsec := int64(t.nsec)
	rem := nsec % int64(d)
	if rem < int64(d)/2 {
		nsec -= rem
	} else {
		nsec += int64(d) - rem
	}
	if nsec >= 1_000_000_000 {
		nsec -= 1_000_000_000
		t.sec++
	} else if nsec < 0 {
		nsec += 1_000_000_000
		t.sec--
	}
	return Time{sec: t.sec, nsec: int32(nsec)}
}

// Truncate truncates the time to a multiple of d
func (t Time) Truncate(d Duration) Time {
	if d <= 0 {
		return t
	}
	nsec := int64(t.nsec)
	nsec -= nsec % int64(d)
	if nsec < 0 {
		nsec += 1_000_000_000
	}
	return Time{sec: t.sec, nsec: int32(nsec)}
}

// Unix returns the time as Unix seconds
func (t Time) Unix() int64 {
	return t.sec
}

// UnixNano returns the time as Unix nanoseconds
func (t Time) UnixNano() int64 {
	return t.sec*1_000_000_000 + int64(t.nsec)
}

// UnixMicro returns the time as Unix microseconds
func (t Time) UnixMicro() int64 {
	return t.sec*1_000_000 + int64(t.nsec)/1000
}

// AddDate returns the time with years, months, days added
func (t Time) AddDate(years, months, days int) Time {
	// Minimal - just add days as seconds
	return t.Add(Duration(days) * 24 * Hour)
}

// Month returns the month of the time
func (t Time) Month() int {
	return 1 // January
}

// Day returns the day of the month
func (t Time) Day() int {
	return 1
}

// Weekday returns the day of the week
func (t Time) Weekday() int {
	return 0 // Sunday
}

// Year returns the year
func (t Time) Year() int {
	return 1970
}

// Hour returns the hour
func (t Time) Hour() int {
	return int((t.sec / 3600) % 24)
}

// Minute returns the minute
func (t Time) Minute() int {
	return int((t.sec / 60) % 60)
}

// Second returns the second
func (t Time) Second() int {
	return int(t.sec % 60)
}

// Nanosecond returns the nanosecond
func (t Time) Nanosecond() int {
	return int(t.nsec)
}

// IsZero reports whether t is the zero time
func (t Time) IsZero() bool {
	return t.sec == 0 && t.nsec == 0
}

// String returns a string representation
func (t Time) String() string {
	return t.Format("15:04:05")
}

// =============================================================================
// Duration Methods
// =============================================================================

// Nanoseconds returns the duration as nanoseconds
func (d Duration) Nanoseconds() int64 {
	return int64(d)
}

// Microseconds returns the duration as microseconds
func (d Duration) Microseconds() int64 {
	return int64(d) / 1000
}

// Milliseconds returns the duration as milliseconds
func (d Duration) Milliseconds() int64 {
	return int64(d) / 1_000_000
}

// Seconds returns the duration as seconds
func (d Duration) Seconds() float64 {
	return float64(d) / 1_000_000_000
}

// Minutes returns the duration as minutes
func (d Duration) Minutes() float64 {
	return float64(d) / 60_000_000_000
}

// Hours returns the duration as hours
func (d Duration) Hours() float64 {
	return float64(d) / 3_600_000_000_000
}

// Round rounds the duration to the nearest multiple of m
func (d Duration) Round(m Duration) Duration {
	if m <= 0 {
		return d
	}
	rem := d % m
	if rem < m/2 {
		return d - rem
	}
	return d + m - rem
}

// Truncate truncates the duration to a multiple of m
func (d Duration) Truncate(m Duration) Duration {
	if m <= 0 {
		return d
	}
	return d - d%m
}

// String returns a string representation of the duration
func (d Duration) String() string {
	// Minimal implementation
	ms := d.Milliseconds()
	return itoa(int(ms)) + "ms"
}

// =============================================================================
// Time Parsing (stubs)
// =============================================================================

// Parse parses a time string according to the layout
func Parse(layout, value string) (Time, error) {
	return Time{}, Errno(ENOSYS)
}

// ParseInLocation parses a time string in the given location
func ParseInLocation(layout, value string, loc *Location) (Time, error) {
	return Time{}, Errno(ENOSYS)
}

// Errno represents a system call error
type Errno uintptr

func (e Errno) Error() string {
	return "syscall error"
}