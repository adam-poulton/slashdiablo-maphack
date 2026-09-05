#pragma once

namespace Drawing {
	// The values a slider will take: the ends of its range, the step between them,
	// and the arithmetic that gets from a value to a position on the rail and back.
	//
	// Apart from the hook that draws it, because this is the part that has to be
	// right. A slider is the control to reach for where a value outside the range
	// would leave the setting unusable, so the rule that holds a value inside it is
	// the whole point of the thing - and a rule that can only be exercised by
	// dragging a thumb across a screen is a rule nobody checks.
	//
	// Positions are counted in steps from the minimum, so the first is 0 and the
	// last is StepCount(). A range that does not divide evenly by its step still
	// reaches its maximum: the last step is a short one rather than the rail
	// stopping below the top of the range.
	struct SliderRange {
		unsigned int min, max, step;

		// A range the wrong way round is taken as the range it names rather than
		// refused, and a step of nothing would leave the rail with no positions on
		// it and every division below by zero.
		SliderRange(unsigned int low, unsigned int high, unsigned int by) {
			min = (low <= high) ? low : high;
			max = (low <= high) ? high : low;
			step = (by > 0) ? by : 1;
		}

		unsigned int StepCount() const {
			unsigned int span = max - min;
			if (span == 0)
				return 0;
			// Rounded up, so the maximum is always the last position.
			return (span + step - 1) / step;
		}

		unsigned int ValueForIndex(unsigned int index) const {
			unsigned int steps = StepCount();
			if (index >= steps)
				return max;
			return min + (index * step);
		}

		// Rounded to the nearer position rather than truncated, so a value from the
		// config that sits between two of them lands on the one it is closest to.
		unsigned int IndexForValue(unsigned int raw) const {
			unsigned int steps = StepCount();
			if (steps == 0 || raw <= min)
				return 0;
			if (raw >= max)
				return steps;

			// Where the range does not divide evenly by the step, the last gap is a
			// short one, and half a step is further than half of it: rounding the
			// top of the range by the step would carry a value that is nearer the
			// maximum back down to the step below it. So the short gap is measured
			// from both its ends instead.
			unsigned int lastFull = min + ((steps - 1) * step);
			if (raw > lastFull)
				return ((raw - lastFull) >= (max - raw)) ? steps : (steps - 1);

			return ((raw - min) + (step / 2)) / step;
		}

		// The value the slider will hold, whatever it was handed. The value belongs
		// to a module, which reads it from a file anybody can edit, so this is the
		// only guarantee that what is drawn is what is held.
		unsigned int Snap(unsigned int raw) const {
			return ValueForIndex(IndexForValue(raw));
		}

		// How far along the rail the thumb has been dragged, in pixels, as a
		// position. Rounds at the halfway point of one step's worth of travel, so
		// the value turns over where it looks like it should. Signed, because a drag
		// carries on past the near end of the rail.
		unsigned int IndexForTravel(int along, unsigned int travel) const {
			unsigned int steps = StepCount();
			if (steps == 0 || travel == 0 || along <= 0)
				return 0;
			if ((unsigned int)along >= travel)
				return steps;
			return (((unsigned int)along * steps) + (travel / 2)) / travel;
		}
	};
};
