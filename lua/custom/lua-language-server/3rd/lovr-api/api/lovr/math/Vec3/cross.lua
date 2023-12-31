return {
  summary = 'Get the cross product with another vector.',
  description = [[
    Sets this vector to be equal to the cross product between this vector and another one.  The
    new `v` will be perpendicular to both the old `v` and `u`.
  ]],
  arguments = {
    u = {
      type = 'Vec3',
      description = 'The vector to compute the cross product with.'
    },
    x = {
      type = 'number',
      description = 'A value of x component to compute cross product with.'
    },
    y = {
      type = 'number',
      description = 'A value of y component to compute cross product with.'
    },
    z = {
      type = 'number',
      description = 'A value of z component to compute cross product with.'
    }
  },
  returns = {
    self = {
      type = 'Vec3',
      description = 'The modified vector.'
    }
  },
  variants = {
    {
      arguments = { 'u' },
      returns = { 'self' }
    },
    {
      arguments = { 'x', 'y', 'z' },
      returns = { 'self' }
    }
  },
  notes = 'The vectors are not normalized before or after computing the cross product.',
  related = {
    'Vec3:dot'
  }
}
