return {
  summary = 'Conjugate (invert) the quaternion.',
  description = [[
    Conjugates the input quaternion in place, returning the input.  If the quaternion is normalized,
    this is the same as inverting it.  It negates the (x, y, z) components of the quaternion.
  ]],
  arguments = {},
  returns = {
    self = {
      type = 'Quat',
      description = 'The inverted quaternion.'
    }
  },
  variants = {
    {
      arguments = {},
      returns = { 'self' }
    }
  }
}
