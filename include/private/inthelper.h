/* SPDX-License-Identifier: Apache-2.0 */

/*
 * Copyright 2018-2019 Joel E. Anderson
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef __STUMPLESS_PRIVATE_INTHELPER_H
#  define __STUMPLESS_PRIVATE_INTHELPER_H

#  include <stddef.h>

/**
 * The maximum possible size of an integer when it is converted to a string.
 *
 * This may be a little heavy-handed, but there is potential for different
 * systems to support different sizes, and without a more precise way to figure
 * this out we'll stick with this.
 */
#  define MAX_INT_SIZE 50

int
cap_size_t_to_int( size_t val );


int
size_t_to_int( size_t val );

#endif /* __STUMPLESS_PRIVATE_INTHELPER_H */
