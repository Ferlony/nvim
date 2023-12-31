return {
  summary = 'Create a view transform that looks from a position to target position.',
  description = [[
    Sets a view transform matrix that moves and orients camera to look at a target point.

    This is useful for changing camera position and orientation. The resulting Mat4 matrix can be
    passed to `lovr.graphics.transform()` directly (without inverting) before rendering the scene.

    The lookAt() function produces same result as target() after matrix inversion.
  ]],
  arguments = {
    from = {
      type = 'Vec3',
      description = 'The position of the viewer.'
    },
    to = {
      type = 'Vec3',
      description = 'The position of the target.'
    },
    up = {
      type = 'Vec3',
      default = 'Vec3(0, 1, 0)',
      description = 'The up vector of the viewer.'
    }
  },
  returns = {
    self = {
      type = 'Mat4',
      description = 'The modified matrix.'
    }
  },
  variants = {
    {
      arguments = { 'from', 'to', 'up' },
      returns = { 'self' }
    }
  },
  related = {
    'Mat4:target',
    'Quat:direction'
  }
}
